#ifndef KOMIKAN_H
#define KOMIKAN_H

#include <pthread.h>
#include <signal.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
#include <queue>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace std;
using Reloj = chrono::steady_clock;

const int CASILLAS = 28;
const int FICHAS = 13;
const int TRONO_IZQ = 25;
const int TRONO_DER = 26;
const int TRONO = 27;

enum Resultado {
    EN_CURSO, GANA_PUMA, GANAN_PERROS, ABANDONA_PUMA,
    ABANDONAN_PERROS, INTERRUMPIDA, SIN_GANADOR, ERROR_INTERNO
};

struct Configuracion {
    int retardoMin = 50;
    int retardoMax = 300;
    int semilla = 2026;
    int maxMovimientos = 1000;
    int duracionMax = 30000;
    int logCada = 20;
    int abandonoPuma = -1;
    int abandonoPerros = -1;
    int falloPuma = -1;
    int falloPerros = -1;
    int tiempoDesconexion = 5000;
    int inicial[FICHAS] = {
        TRONO, 10, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24
    };
};

struct Pareja {
    int puma;
    int perros;
    int numero;
};

struct Cliente {
    pthread_cond_t aviso;
    bool terminado = false;
    bool sinPareja = false;
    bool conectado = true;
    bool esPuma = false;
    Resultado resultado = EN_CURSO;
    int partida = 0;
    Reloj::time_point ultimoLatido;
    Reloj::time_point emparejado;
};

struct Servidor {
    int total = 0;
    int maxMesas = 0;
    int llegados = 0;
    int esperando = -1;
    int partidas = 0;
    int terminadas = 0;
    int activas = 0;
    int maxActivas = 0;
    bool fin = false;
    atomic<bool> detener{false};
    Configuracion config;
    queue<Pareja> cola;
    vector<Cliente> clientes;
    pthread_mutex_t mutex;
    pthread_mutex_t salida;
    pthread_cond_t hayParejas;
};

struct Movimiento {
    int origen;
    int destino;
    int saltado;
};

struct Solicitud {
    int ficha;
    Movimiento movimiento;
};

struct Mesa {
    Servidor* servidor;
    Pareja pareja;
    int numero;
    int tablero[CASILLAS];
    int posicion[FICHAS];
    bool pendiente[FICHAS] = {};
    queue<Solicitud> solicitudes;
    int capturas = 0;
    int movimientos = 0;
    int rechazados = 0;
    bool terminada = false;
    Resultado resultado = EN_CURSO;
    Reloj::time_point inicio;
    pthread_mutex_t mutex;
    pthread_cond_t cambio;
    pthread_cond_t hayTrabajo;
};

struct DatosCliente { Servidor* servidor; int id; };
struct DatosJuez { Servidor* servidor; int numero; };
struct DatosFicha { Mesa* mesa; int id; };
struct DatosSenal { Servidor* servidor; sigset_t senales; };

extern bool caminos[CASILLAS][CASILLAS];
extern int columna[CASILLAS];
extern int fila[CASILLAS];

void conectar(int a, int b);
void crearTablero();
string limpiar(string texto);
bool leerEntero(const string& texto, int& numero);
bool leerPosicion(string texto, int& posicion);
bool cargarConfig(Configuracion& config, const string& archivo);
string nombreResultado(Resultado resultado);
char simbolo(int valor);
void mostrarMesa(Mesa& mesa, const string& motivo);
bool saltoRecto(int origen, int medio, int destino);
bool esValido(const Mesa& mesa, int ficha, const Movimiento& mov);
vector<Movimiento> movimientosValidos(const Mesa& mesa, int ficha);
bool tableroCorrecto(const Mesa& mesa);
void aplicarMovimiento(Mesa& mesa, int ficha, const Movimiento& mov);
void dormir(int milisegundos, const atomic<bool>& detener);
void* moverFicha(void* dato);
int tiempoPasado(const Mesa& mesa);
void revisarPartida(Mesa& mesa);
Resultado jugar(Servidor& servidor, Pareja pareja, int numeroMesa);
void* atenderMesa(void* dato);
void* recibirCliente(void* dato);
void despertarTodos(Servidor& servidor);
void* esperarSenal(void* dato);

#endif
