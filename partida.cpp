#include "komikan.h"

void dormir(int milisegundos, const atomic<bool>& detener) {
    while (milisegundos > 0 && !detener.load()) {
        int tramo = min(milisegundos, 10);
        this_thread::sleep_for(chrono::milliseconds(tramo));
        milisegundos -= tramo;
    }
}

void* moverFicha(void* dato) {
    DatosFicha* datos = static_cast<DatosFicha*>(dato);
    Mesa& mesa = *datos->mesa;
    int id = datos->id;
    const Configuracion& config = mesa.servidor->config;
    mt19937 azar(static_cast<unsigned int>(config.semilla) +
                 static_cast<unsigned int>(mesa.pareja.numero) * 101u +
                 static_cast<unsigned int>(id) * 37u);
    uniform_int_distribution<int> pausa(config.retardoMin, config.retardoMax);

    while (!mesa.servidor->detener.load()) {
        dormir(pausa(azar), mesa.servidor->detener);
        if (mesa.servidor->detener.load()) break;

        pthread_mutex_lock(&mesa.mutex);
        if (mesa.terminada || mesa.posicion[id] == -1) {
            pthread_mutex_unlock(&mesa.mutex);
            break;
        }
        vector<Movimiento> opciones = movimientosValidos(mesa, id);
        if (!opciones.empty()) {
            uniform_int_distribution<int> elegir(0, static_cast<int>(opciones.size()) - 1);
            mesa.pendiente[id] = true;
            mesa.solicitudes.push({id, opciones[elegir(azar)]});
            pthread_cond_signal(&mesa.cambio);
            while (mesa.pendiente[id] && !mesa.terminada)
                pthread_cond_wait(&mesa.cambio, &mesa.mutex);
        }
        bool terminar = mesa.terminada;
        pthread_mutex_unlock(&mesa.mutex);
        if (terminar) break;
    }
    return nullptr;
}

int tiempoPasado(const Mesa& mesa) {
    return static_cast<int>(chrono::duration_cast<chrono::milliseconds>(
        Reloj::now() - mesa.inicio).count());
}

void revisarPartida(Mesa& mesa) {
    if (mesa.terminada) return;
    const Configuracion& config = mesa.servidor->config;

    if (mesa.servidor->detener.load()) {
        mesa.resultado = INTERRUMPIDA;
    } else {
        bool pumaDesconectado;
        bool perrosDesconectados;
        auto ahora = Reloj::now();
        pthread_mutex_lock(&mesa.servidor->mutex);
        const Cliente& puma = mesa.servidor->clientes[mesa.pareja.puma];
        const Cliente& perros = mesa.servidor->clientes[mesa.pareja.perros];
        pumaDesconectado = !puma.conectado ||
            chrono::duration_cast<chrono::milliseconds>(ahora - puma.ultimoLatido).count()
            > config.tiempoDesconexion;
        perrosDesconectados = !perros.conectado ||
            chrono::duration_cast<chrono::milliseconds>(ahora - perros.ultimoLatido).count()
            > config.tiempoDesconexion;
        pthread_mutex_unlock(&mesa.servidor->mutex);

        if (pumaDesconectado) mesa.resultado = ABANDONA_PUMA;
        else if (perrosDesconectados) mesa.resultado = ABANDONAN_PERROS;
        else if (mesa.capturas >= 6) mesa.resultado = GANA_PUMA;
        else if (movimientosValidos(mesa, 0).empty()) mesa.resultado = GANAN_PERROS;
        else if ((config.maxMovimientos > 0 && mesa.movimientos >= config.maxMovimientos) ||
                 (config.duracionMax > 0 && tiempoPasado(mesa) >= config.duracionMax))
            mesa.resultado = SIN_GANADOR;
    }

    if (mesa.resultado != EN_CURSO) {
        mesa.terminada = true;
        pthread_cond_broadcast(&mesa.cambio);
    }
}

static void esperarCambio(Mesa& mesa) {
    timespec limite;
    clock_gettime(CLOCK_REALTIME, &limite);
    limite.tv_nsec += 50000000L;
    if (limite.tv_nsec >= 1000000000L) {
        limite.tv_sec++;
        limite.tv_nsec -= 1000000000L;
    }
    pthread_cond_timedwait(&mesa.cambio, &mesa.mutex, &limite);
}

Resultado jugar(Servidor& servidor, Pareja pareja, int numeroMesa) {
    Mesa mesa{};
    mesa.servidor = &servidor;
    mesa.pareja = pareja;
    mesa.numero = numeroMesa;
    pthread_mutex_init(&mesa.mutex, nullptr);
    pthread_cond_init(&mesa.cambio, nullptr);
    fill(mesa.tablero, mesa.tablero + CASILLAS, -1);
    for (int i = 0; i < FICHAS; i++) {
        mesa.posicion[i] = servidor.config.inicial[i];
        mesa.tablero[mesa.posicion[i]] = i;
    }
    mesa.inicio = Reloj::now();
    mostrarMesa(mesa, "Inicio");

    pthread_t hilos[FICHAS];
    DatosFicha datos[FICHAS];
    int creados = 0;
    for (int i = 0; i < FICHAS; i++) {
        if (servidor.detener.load()) break;
        datos[i] = {&mesa, i};
        if (pthread_create(&hilos[i], nullptr, moverFicha, &datos[i]) != 0) {
            pthread_mutex_lock(&mesa.mutex);
            mesa.resultado = (i == 0) ? ABANDONA_PUMA : ABANDONAN_PERROS;
            mesa.terminada = true;
            pthread_cond_broadcast(&mesa.cambio);
            pthread_mutex_unlock(&mesa.mutex);
            break;
        }
        creados++;
    }

    int ultimoLog = 0;
    int ultimasCapturas = 0;
    pthread_mutex_lock(&mesa.mutex);
    revisarPartida(mesa);
    while (!mesa.terminada) {
        if (mesa.solicitudes.empty()) {
            esperarCambio(mesa);
        } else {
            Solicitud solicitud = mesa.solicitudes.front();
            mesa.solicitudes.pop();
            mesa.pendiente[solicitud.ficha] = false;
            if (esValido(mesa, solicitud.ficha, solicitud.movimiento)) {
                aplicarMovimiento(mesa, solicitud.ficha, solicitud.movimiento);
                if (!tableroCorrecto(mesa)) {
                    mesa.resultado = ERROR_INTERNO;
                    mesa.terminada = true;
                }
                if (mesa.capturas != ultimasCapturas ||
                    mesa.movimientos - ultimoLog >= servidor.config.logCada) {
                    mostrarMesa(mesa, mesa.capturas != ultimasCapturas ? "Captura" : "Movimiento");
                    ultimasCapturas = mesa.capturas;
                    ultimoLog = mesa.movimientos;
                }
            } else {
                mesa.rechazados++;
            }
            pthread_cond_broadcast(&mesa.cambio);
        }
        revisarPartida(mesa);
    }
    pthread_cond_broadcast(&mesa.cambio);
    pthread_mutex_unlock(&mesa.mutex);

    for (int i = 0; i < creados; i++) pthread_join(hilos[i], nullptr);
    pthread_mutex_lock(&mesa.mutex);
    mostrarMesa(mesa, nombreResultado(mesa.resultado));
    Resultado resultado = mesa.resultado;
    pthread_mutex_unlock(&mesa.mutex);

    pthread_cond_destroy(&mesa.cambio);
    pthread_mutex_destroy(&mesa.mutex);
    return resultado;
}
