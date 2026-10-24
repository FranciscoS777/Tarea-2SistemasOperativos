#include "komikan.h"
#include <cassert>

int main() {
    crearTablero();
    assert(caminos[TRONO][TRONO_IZQ]);
    assert(caminos[TRONO][TRONO_DER]);
    assert(caminos[TRONO][2]);
    assert(!caminos[0][6]);
    assert(!caminos[0][TRONO]);

    Mesa mesa{};
    fill(mesa.tablero, mesa.tablero + CASILLAS, -1);
    int inicio[FICHAS] = {10, 11, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24};
    for (int i = 0; i < FICHAS; i++) {
        mesa.posicion[i] = inicio[i];
        mesa.tablero[inicio[i]] = i;
    }
    assert(tableroCorrecto(mesa));
    assert(esValido(mesa, 0, {10, 12, 11}));
    assert(!esValido(mesa, 1, {11, 13, 12}));
    assert(!esValido(mesa, 0, {10, 6, -1}));

    aplicarMovimiento(mesa, 0, {10, 12, 11});
    assert(mesa.capturas == 1);
    assert(mesa.movimientos == 1);
    assert(mesa.posicion[1] == -1);
    assert(mesa.tablero[10] == -1);
    assert(mesa.tablero[11] == -1);
    assert(mesa.tablero[12] == 0);
    assert(!esValido(mesa, 1, {-1, 6, -1}));
    assert(tableroCorrecto(mesa));
    cout << "OK: tablero, movimientos, captura y consistencia.\n";
}
