#include "komikan.h"

int main(int argc, char* argv[]) {
    if (argc < 3 || argc > 4) {
        cerr << "Uso: ./torneo N M [config.txt]" << endl;
        return 1;
    }

    int n, m;
    if (!leerEntero(argv[1], n) || !leerEntero(argv[2], m) ||
        n < 1 || n > 10000 || m < 1 || m > 10000) {
        cerr << "N y M deben ser positivos y no superar 10000" << endl;
        return 1;
    }

    crearTablero();

    Servidor servidor;
    servidor.total = n;
    servidor.maxMesas = m;

    if (!cargarConfig(servidor.config, argc == 4 ? argv[3] : ""))
        return 1;

    servidor.clientes.resize(n);

    pthread_mutex_init(&servidor.mutex, nullptr);
    pthread_mutex_init(&servidor.salida, nullptr);
    pthread_cond_init(&servidor.hayParejas, nullptr);

    for (Cliente& cliente : servidor.clientes)
        pthread_cond_init(&cliente.aviso, nullptr);

    sigset_t senales;
    sigemptyset(&senales);
    sigaddset(&senales, SIGINT);
    sigaddset(&senales, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &senales, nullptr);

    DatosSenal datosSenal{&servidor, senales};
    pthread_t hiloSenal;

    if (pthread_create(&hiloSenal, nullptr, esperarSenal, &datosSenal) != 0) {
        cerr << "No se pudo iniciar el servidor" << endl;

        for (Cliente& cliente : servidor.clientes)
            pthread_cond_destroy(&cliente.aviso);

        pthread_cond_destroy(&servidor.hayParejas);
        pthread_mutex_destroy(&servidor.salida);
        pthread_mutex_destroy(&servidor.mutex);

        return 1;
    }

    int cantidadMesas = min(m, n / 2);

    vector<pthread_t> jueces(cantidadMesas);
    vector<DatosJuez> datosJueces(cantidadMesas);
    vector<pthread_t> hilosClientes(n);
    vector<DatosCliente> datosClientes(n);

    int juecesCreados = 0;
    int clientesCreados = 0;
    int error = 0;

    pthread_attr_t atributos;
    pthread_attr_init(&atributos);
    pthread_attr_setstacksize(&atributos, 256 * 1024);

    cout << "Servidor iniciado: " << n << " clientes, "
         << m << " mesas maximas" << endl;

    for (int i = 0; i < cantidadMesas && !servidor.detener.load(); i++) {
        datosJueces[i] = {&servidor, i + 1};

        if (pthread_create(&jueces[i], &atributos, atenderMesa,
                           &datosJueces[i]) != 0) {
            error = 1;
            break;
        }

        juecesCreados++;
    }

    if (juecesCreados == cantidadMesas) {
        for (int i = 0; i < n && !servidor.detener.load(); i++) {
            datosClientes[i] = {&servidor, i};

            if (pthread_create(&hilosClientes[i], &atributos, recibirCliente,
                               &datosClientes[i]) != 0) {
                error = 1;
                break;
            }

            clientesCreados++;
        }
    } else if (!servidor.detener.load()) {
        error = 1;
    }

    pthread_attr_destroy(&atributos);

    if (error) {
        cerr << "No se pudieron crear todos los hilos" << endl;
        servidor.detener.store(true);
        despertarTodos(servidor);
    }

    for (int i = 0; i < clientesCreados; i++)
        pthread_join(hilosClientes[i], nullptr);

    pthread_mutex_lock(&servidor.mutex);
    servidor.fin = true;
    pthread_cond_broadcast(&servidor.hayParejas);
    pthread_mutex_unlock(&servidor.mutex);

    for (int i = 0; i < juecesCreados; i++)
        pthread_join(jueces[i], nullptr);

    pthread_kill(hiloSenal, SIGUSR1);
    pthread_join(hiloSenal, nullptr);

    cout << endl << "Resumen: " << servidor.terminadas
         << " partidas finalizadas, " << servidor.maxActivas
         << " mesas activas como maximo" << endl;

    for (Cliente& cliente : servidor.clientes)
        pthread_cond_destroy(&cliente.aviso);

    pthread_cond_destroy(&servidor.hayParejas);
    pthread_mutex_destroy(&servidor.salida);
    pthread_mutex_destroy(&servidor.mutex);

    return error ? 1 : (servidor.detener.load() ? 130 : 0);
}
