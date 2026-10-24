#include <iostream>
#include <cstdlib>

using namespace std;

int main(int argc, char* argv[]) {

    if (argc < 3 || argc > 4) {
        cout << "Uso: ./torneo N M [config.txt]" << endl;
        return 1;
    }

    int clientes = atoi(argv[1]);
    int mesas = atoi(argv[2]);

    if (clientes <= 0 || mesas <= 0) {
        cout << "Los valores deben ser mayores que cero" << endl;
        return 1;
    }

    cout << "Servidor iniciado" << endl;
    cout << "Clientes: " << clientes << endl;
    cout << "Mesas maximas: " << mesas << endl;

    return 0;
}