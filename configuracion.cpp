
#include "komikan.h"
#include <cctype>

string limpiar(string texto) {
    while (!texto.empty() && isspace(static_cast<unsigned char>(texto.front()))) {
        texto.erase(0, 1);
    }
    while (!texto.empty() && isspace(static_cast<unsigned char>(texto.back()))) {
        texto.pop_back();
    }
    return texto;
}

bool leerEntero(const string& texto, int& numero) {
    stringstream datos(texto);
    char sobrante;
    if (!(datos >> numero)) return false;
    if (datos >> sobrante) return false;
    return true;
}

bool leerPosicion(string texto, int& posicion) {
    texto = limpiar(texto);

    if (texto == "T") { posicion = TRONO; return true; }
    if (texto == "A") { posicion = TRONO_IZQ; return true; }
    if (texto == "B") { posicion = TRONO_DER; return true; }

    stringstream datos(texto);
    int fila, columna;
    char coma, sobrante;

    if (!(datos >> fila >> coma >> columna)) return false;
    if (coma != ',' || (datos >> sobrante)) return false;
    if (fila < 0 || fila > 4 || columna < 0 || columna > 4) return false;

    posicion = fila * 5 + columna;
    return true;
}

bool cargarConfig(Configuracion& config, const string& archivo) {
    if (archivo.empty()) return true;

    ifstream entrada(archivo);
    if (!entrada) {
        cerr << "No se pudo abrir " << archivo << endl;
        return false;
    }

    string linea;
    int numeroLinea = 0;

    while (getline(entrada, linea)) {
        numeroLinea++;

        stringstream sinComentario(linea);
        getline(sinComentario, linea, '#');
        linea = limpiar(linea);
        if (linea.empty()) continue;

        stringstream datos(linea);
        string clave, valor;
        if (!getline(datos, clave, '=') || !getline(datos, valor)) {
            cerr << "Error en linea " << numeroLinea << endl;
            return false;
        }

        clave = limpiar(clave);
        valor = limpiar(valor);
        bool correcto = true;
        int numero = 0;

        if (clave == "puma") {
            correcto = leerPosicion(valor, config.inicial[0]);
        } else if (clave == "perros") {
            stringstream posiciones(valor);
            string parte;
            int cantidad = 0;

            while (getline(posiciones, parte, ';')) {
                int posicion;
                if (cantidad == 12 || !leerPosicion(parte, posicion)) {
                    correcto = false;
                    break;
                }
                cantidad++;
                config.inicial[cantidad] = posicion;
            }
            if (cantidad != 12) correcto = false;
        } else if (!leerEntero(valor, numero)) {
            correcto = false;
        } else if (clave == "retardo_min_ms") config.retardoMin = numero;
        else if (clave == "retardo_max_ms") config.retardoMax = numero;
        else if (clave == "seed") config.semilla = numero;
        else if (clave == "max_movimientos") config.maxMovimientos = numero;
        else if (clave == "duracion_max_ms") config.duracionMax = numero;
        else if (clave == "log_cada") config.logCada = numero;
        else if (clave == "abandono_puma_ms") config.abandonoPuma = numero;
        else if (clave == "abandono_perros_ms") config.abandonoPerros = numero;
        else if (clave == "fallo_puma_ms") config.falloPuma = numero;
        else if (clave == "fallo_perros_ms") config.falloPerros = numero;
        else if (clave == "tiempo_desconexion_ms") config.tiempoDesconexion = numero;
        else correcto = false;

        if (!correcto) {
            cerr << "Dato incorrecto en linea " << numeroLinea << endl;
            return false;
        }
    }

    if (config.retardoMin < 1 || config.retardoMax < config.retardoMin ||
        config.retardoMax > 60000 || config.maxMovimientos < 0 ||
        config.duracionMax < 0 || config.logCada < 1 ||
        config.tiempoDesconexion < 500 ||
        config.abandonoPuma < -1 || config.abandonoPerros < -1 ||
        config.falloPuma < -1 || config.falloPerros < -1) {
        cerr << "Valores de configuracion fuera de rango" << endl;
        return false;
    }

    bool usados[CASILLAS] = {};
    for (int i = 0; i < FICHAS; i++) {
        int posicion = config.inicial[i];
        if (posicion < 0 || posicion >= CASILLAS || usados[posicion]) {
            cerr << "Las 13 fichas necesitan posiciones distintas" << endl;
            return false;
        }
        usados[posicion] = true;
    }
    return true;
}
