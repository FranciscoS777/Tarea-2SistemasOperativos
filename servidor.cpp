#include "komikan.h"

static void limpiarCliente(void* dato) {
    DatosCliente* datos = static_cast<DatosCliente*>(dato);
    Servidor& servidor = *datos->servidor;
    servidor.clientes[datos->id].conectado = false;
    pthread_mutex_unlock(&servidor.mutex);
}

static void esperarCliente(Cliente& cliente, pthread_mutex_t& mutex) {
    timespec limite;
    clock_gettime(CLOCK_REALTIME, &limite);
    limite.tv_nsec += 100000000L;
    if (limite.tv_nsec >= 1000000000L) {
        limite.tv_sec++;
        limite.tv_nsec -= 1000000000L;
    }
    pthread_cond_timedwait(&cliente.aviso, &mutex, &limite);
}

void* atenderMesa(void* dato) {
    DatosJuez* datos = static_cast<DatosJuez*>(dato);
    Servidor& servidor = *datos->servidor;
    while (true) {
        pthread_mutex_lock(&servidor.mutex);
        while (servidor.cola.empty() && !servidor.fin && !servidor.detener.load())
            pthread_cond_wait(&servidor.hayParejas, &servidor.mutex);
        if (servidor.detener.load() || (servidor.fin && servidor.cola.empty())) {
            pthread_mutex_unlock(&servidor.mutex);
            break;
        }
        Pareja pareja = servidor.cola.front();
        servidor.cola.pop();
        servidor.activas++;
        servidor.maxActivas = max(servidor.maxActivas, servidor.activas);
        pthread_mutex_unlock(&servidor.mutex);

        Resultado resultado = jugar(servidor, pareja, datos->numero);

        pthread_mutex_lock(&servidor.mutex);
        servidor.activas--;
        servidor.terminadas++;
        for (int id : {pareja.puma, pareja.perros}) {
            Cliente& cliente = servidor.clientes[id];
            cliente.resultado = resultado;
            cliente.terminado = true;
            pthread_cond_signal(&cliente.aviso);
        }
        pthread_mutex_unlock(&servidor.mutex);
    }
    return nullptr;
}

void* recibirCliente(void* dato) {
    DatosCliente* datos = static_cast<DatosCliente*>(dato);
    Servidor& servidor = *datos->servidor;
    int id = datos->id;
    mt19937 azar(static_cast<unsigned int>(servidor.config.semilla) +
                 static_cast<unsigned int>(id) * 13u);
    dormir(uniform_int_distribution<int>(0, 20)(azar), servidor.detener);

    bool terminado = false;
    bool sinPareja = false;
    int partida = 0;
    Resultado resultado = EN_CURSO;
    bool abandono = false;
    pthread_mutex_lock(&servidor.mutex);
    pthread_cleanup_push(limpiarCliente, dato);
    Cliente& cliente = servidor.clientes[id];
    cliente.ultimoLatido = Reloj::now();
    if (!servidor.detener.load()) {
        servidor.llegados++;
        if (servidor.esperando == -1) {
            servidor.esperando = id;
        } else {
            Pareja pareja{servidor.esperando, id, ++servidor.partidas};
            servidor.esperando = -1;
            servidor.cola.push(pareja);
            for (int jugador : {pareja.puma, pareja.perros}) {
                Cliente& otro = servidor.clientes[jugador];
                otro.partida = pareja.numero;
                otro.esPuma = jugador == pareja.puma;
                otro.emparejado = Reloj::now();
                if ((otro.esPuma ? servidor.config.abandonoPuma :
                                    servidor.config.abandonoPerros) == 0)
                    otro.conectado = false;
                pthread_cond_signal(&otro.aviso);
            }
            pthread_cond_signal(&servidor.hayParejas);
        }
        if (servidor.llegados == servidor.total && servidor.esperando != -1) {
            servidor.clientes[servidor.esperando].sinPareja = true;
            pthread_cond_signal(&servidor.clientes[servidor.esperando].aviso);
            servidor.esperando = -1;
        }
    }

    while (!servidor.detener.load() && !cliente.terminado && !cliente.sinPareja) {
        if (!cliente.conectado) {
            abandono = true;
            break;
        }
        cliente.ultimoLatido = Reloj::now();
        if (cliente.partida != 0) {
            int limite = cliente.esPuma ? servidor.config.abandonoPuma :
                                        servidor.config.abandonoPerros;
            int fallo = cliente.esPuma ? servidor.config.falloPuma :
                                       servidor.config.falloPerros;
            int tiempo = static_cast<int>(chrono::duration_cast<chrono::milliseconds>(
                Reloj::now() - cliente.emparejado).count());
            if (fallo >= 0 && tiempo >= fallo) pthread_exit(nullptr);
            if (limite >= 0 && tiempo >= limite) {
                cliente.conectado = false;
                abandono = true;
                break;
            }
        }
        esperarCliente(cliente, servidor.mutex);
    }
    terminado = cliente.terminado;
    sinPareja = cliente.sinPareja;
    partida = cliente.partida;
    resultado = cliente.resultado;
    pthread_cleanup_pop(0);
    pthread_mutex_unlock(&servidor.mutex);

    pthread_mutex_lock(&servidor.salida);
    if (abandono)
        cout << "[Cliente " << id + 1 << "] Se desconecto\n";
    else if (terminado)
        cout << "[Cliente " << id + 1 << " | Partida " << partida << "] "
             << nombreResultado(resultado) << "\n";
    else if (sinPareja)
        cout << "[Cliente " << id + 1 << "] Sin pareja\n";
    else
        cout << "[Cliente " << id + 1 << "] Salio por interrupcion\n";
    pthread_mutex_unlock(&servidor.salida);
    return nullptr;
}

void despertarTodos(Servidor& servidor) {
    pthread_mutex_lock(&servidor.mutex);
    pthread_cond_broadcast(&servidor.hayParejas);
    for (Cliente& cliente : servidor.clientes)
        pthread_cond_broadcast(&cliente.aviso);
    pthread_mutex_unlock(&servidor.mutex);
}

void* esperarSenal(void* dato) {
    DatosSenal* datos = static_cast<DatosSenal*>(dato);
    int senal = 0;
    if (sigwait(&datos->senales, &senal) == 0 && senal == SIGINT) {
        datos->servidor->detener.store(true);
        despertarTodos(*datos->servidor);
        pthread_mutex_lock(&datos->servidor->salida);
        cout << "\nCtrl+C recibido. Cerrando las partidas...\n";
        pthread_mutex_unlock(&datos->servidor->salida);
    }
    return nullptr;
}
