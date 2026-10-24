#include "komikan.h"

bool caminos[CASILLAS][CASILLAS] = {};
int columna[CASILLAS] = {
    0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4,
    0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 1, 3, 2
};
int fila[CASILLAS] = {
    0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 4, 4, 4, 4, 4, -1, -1, -2
};

void conectar(int a, int b) {
    caminos[a][b] = true;
    caminos[b][a] = true;
}

void crearTablero() {
    for (int f = 0; f < 5; f++) {
        for (int c = 0; c < 5; c++) {
            int p = f * 5 + c;
            if (c < 4) conectar(p, p + 1);
            if (f < 4) conectar(p, p + 5);
        }
    }
    conectar(TRONO, TRONO_IZQ);
    conectar(TRONO, TRONO_DER);
    conectar(TRONO, 2);
    conectar(TRONO_IZQ, TRONO_DER);
    conectar(TRONO_IZQ, 0);
    conectar(TRONO_IZQ, 1);
    conectar(TRONO_DER, 3);
    conectar(TRONO_DER, 4);
}


string nombreResultado(Resultado resultado) {
    switch (resultado) {
        case GANA_PUMA: return "Gana el Puma: capturo 6 perros";
        case GANAN_PERROS: return "Ganan los Perros: Puma acorralado";
        case ABANDONA_PUMA: return "Ganan los Perros por abandono del Puma";
        case ABANDONAN_PERROS: return "Gana el Puma por abandono de los Perros";
        case INTERRUMPIDA: return "Partida interrumpida";
        case SIN_GANADOR: return "Sin ganador: limite de simulacion";
        case ERROR_INTERNO: return "Partida cancelada por error interno";
        default: return "En curso";
    }
}

char simbolo(int valor) {
    if (valor == 0) return 'P';
    if (valor > 0) return 'D';
    return '.';
}

void mostrarMesa(Mesa& mesa, const string& motivo) {
    pthread_mutex_lock(&mesa.servidor->salida);
    cout << "\n[Mesa " << mesa.numero << " | Partida " << mesa.pareja.numero
         << "] " << motivo << " | movimientos: " << mesa.movimientos
         << " | capturas: " << mesa.capturas
         << " | conflictos: " << mesa.rechazados << "\n";
    cout << "      " << simbolo(mesa.tablero[TRONO]) << "\n";
    cout << "    " << simbolo(mesa.tablero[TRONO_IZQ]) << "   "
         << simbolo(mesa.tablero[TRONO_DER]) << "\n";
    for (int f = 0; f < 5; f++) {
        cout << "  ";
        for (int c = 0; c < 5; c++) cout << simbolo(mesa.tablero[f * 5 + c]) << " ";
        cout << "\n";
    }
    cout.flush();
    pthread_mutex_unlock(&mesa.servidor->salida);
}

bool saltoRecto(int origen, int medio, int destino) {
    if (!caminos[origen][medio] || !caminos[medio][destino] || origen == destino)
        return false;
    int x1 = columna[medio] - columna[origen];
    int y1 = fila[medio] - fila[origen];
    int x2 = columna[destino] - columna[medio];
    int y2 = fila[destino] - fila[medio];
    return x1 * y2 == y1 * x2 && x1 * x2 + y1 * y2 > 0;
}

bool esValido(const Mesa& mesa, int ficha, const Movimiento& mov) {
    if (ficha < 0 || ficha >= FICHAS || mov.origen < 0 ||
        mov.origen >= CASILLAS || mesa.posicion[ficha] != mov.origen ||
        mov.destino < 0 || mov.destino >= CASILLAS ||
        mesa.tablero[mov.destino] != -1) return false;

    if (mov.saltado == -1) return caminos[mov.origen][mov.destino];
    if (ficha != 0 || mov.saltado < 0 || mov.saltado >= CASILLAS)
        return false;
    return mesa.tablero[mov.saltado] > 0 &&
           saltoRecto(mov.origen, mov.saltado, mov.destino);
}

vector<Movimiento> movimientosValidos(const Mesa& mesa, int ficha) {
    vector<Movimiento> posibles;
    int origen = mesa.posicion[ficha];
    if (origen == -1) return posibles;

    for (int destino = 0; destino < CASILLAS; destino++) {
        if (caminos[origen][destino] && mesa.tablero[destino] == -1)
            posibles.push_back({origen, destino, -1});
        if (ficha != 0 || mesa.tablero[destino] != -1) continue;
        for (int medio = 0; medio < CASILLAS; medio++) {
            if (mesa.tablero[medio] > 0 && saltoRecto(origen, medio, destino))
                posibles.push_back({origen, destino, medio});
        }
    }
    return posibles;
}


bool tableroCorrecto(const Mesa& mesa) {
    bool encontrados[FICHAS] = {};
    int perros = 0;
    for (int casilla = 0; casilla < CASILLAS; casilla++) {
        int ficha = mesa.tablero[casilla];
        if (ficha == -1) continue;
        if (ficha < 0 || ficha >= FICHAS || encontrados[ficha] ||
            mesa.posicion[ficha] != casilla) return false;
        encontrados[ficha] = true;
        if (ficha > 0) perros++;
    }
    if (!encontrados[0] || perros + mesa.capturas != 12) return false;
    for (int i = 0; i < FICHAS; i++) {
        if ((mesa.posicion[i] == -1) == encontrados[i]) return false;
    }
    return true;
}

void aplicarMovimiento(Mesa& mesa, int ficha, const Movimiento& mov) {
    mesa.tablero[mov.origen] = -1;
    mesa.tablero[mov.destino] = ficha;
    mesa.posicion[ficha] = mov.destino;
    if (mov.saltado != -1) {
        int perro = mesa.tablero[mov.saltado];
        mesa.tablero[mov.saltado] = -1;
        mesa.posicion[perro] = -1;
        mesa.capturas++;
    }
    mesa.movimientos++;
}
