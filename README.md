# Tarea 2: Torneo de Komikan - Sistemas Operativos

## Integrantes
* Francisco San Martín
* Felipe Silva

## Descripción del Proyecto
Este proyecto implementa un **servidor multihilo de torneo para el juego de tablero Komikan** en C++. El sistema utiliza la biblioteca Pthreads (hilos POSIX) para gestionar de forma concurrente la llegada de clientes (jugadores), el emparejamiento automático y la simulación de partidas. Dentro de cada partida, cada ficha (el puma y los 12 perros) es controlada por un hilo independiente, empleando mecanismos de sincronización como mutexes y variables de condición para evitar condiciones de carrera y gestionar el estado compartido del tablero de forma segura.

---

## Requisitos e Instalación

### Requisitos del Sistema
* OS: Linux (Ubuntu 20.04 LTS o superior recomendado).
* Compilador: `g++` con soporte para el estándar C++17.
* Utilidades: `make`, `bash` (para el script de pruebas).

### Compilación
Para compilar el proyecto se ha provisto un `Makefile`. En la terminal, dentro del directorio del proyecto, ejecute el siguiente comando:

```bash
make
```
*(También puede ejecutar `make pruebas` para correr los test unitarios y de integración).*

## Modo de uso
El ejecutable recibe tres argumentos: la cantidad total de clientes esperados ($N$), la cantidad máxima de mesas simultáneas ($M$) y, opcionalmente, la ruta a un archivo de configuración:

```bash
./torneo <N_clientes> <M_mesas> [archivo_config]

# Ejemplo de uso con 10 clientes, 5 mesas y un archivo de configuración
./torneo 10 5 config.txt
```

## Formato del archivo de configuración (opcional)
El archivo de texto permite definir reglas y condiciones del torneo mediante pares `clave=valor`. Las líneas que inician con `#` son comentarios. 

Principales parámetros configurables:
* `puma`: Posición inicial del puma (ej: `T` para trono, o coordenadas `f,c`).
* `perros`: Lista de 12 posiciones iniciales separadas por punto y coma (ej: `0,0;0,1;...`).
* `retardo_min_ms` / `retardo_max_ms`: Rango de tiempo (milisegundos) que duerme el hilo de una ficha antes de intentar moverse.
* `seed`: Semilla para la generación de números aleatorios.
* `max_movimientos`: Límite máximo de movimientos permitidos antes de declarar empate.
* `duracion_max_ms`: Duración máxima de la partida en milisegundos.
* `abandono_puma_ms` / `abandono_perros_ms`: Tiempo de inactividad antes de considerar desconexión/abandono del jugador.

## Funciones implementadas
  
* `recibirCliente(void* dato)`: Hilo que simula la conexión de un jugador. Espera un tiempo aleatorio, se registra en el servidor y espera mediante variables de condición (`pthread_cond_wait`) a ser emparejado y, posteriormente, a que finalice su partida.

* `atenderMesa(void* dato)`: Función ejecutada por los hilos "jueces". Esperan a que haya parejas disponibles en la cola del servidor, las retiran y ejecutan el ciclo de la partida llamando a `jugar()`.

* `jugar(Servidor& servidor, Pareja pareja, int numeroMesa)`: Configura el estado inicial del tablero de Komikan y lanza concurrentemente 13 hilos adicionales (`moverFicha`) correspondientes al puma y los 12 perros. Luego se encarga de procesar las solicitudes de movimiento que los hilos proponen.

* `moverFicha(void* dato)`: Hilo que representa la "IA" de una ficha. Duerme un periodo aleatorio y, tras despertar, calcula sus movimientos válidos y envía una solicitud a la mesa (juez), esperando confirmación.

* `cargarConfig(Configuracion& config, const string& archivo)`: Parsea el archivo de texto validando posiciones, evitando solapamientos y verificando que los tiempos entregados estén dentro de rangos coherentes.

* `esperarSenal(void* dato)`: Hilo dedicado de forma exclusiva a interceptar y manejar la señal `SIGINT` (Ctrl+C) utilizando `sigwait`. Permite que el servidor termine todos los hilos de manera controlada y limpie la memoria.

## Decisiones de diseño tomadas

* `1) Uso de Hilos sobre Procesos`: A diferencia de la Tarea 1, se utilizó `pthread` para modelar toda la concurrencia. Esto facilita compartir recursos críticos, como la cola de emparejamientos del `Servidor` y el `Tablero` de cada mesa, sin necesidad de memoria compartida explícita o pipes complejos.

* `2) Mutex y Variables de Condición`: Para la sincronización de hilos y prevención de *busy-waiting* (espera activa), se hizo uso extensivo de `pthread_mutex_t` y `pthread_cond_t`. Por ejemplo, las mesas esperan bloqueadas (`pthread_cond_wait`) hasta que se agregue una pareja a la cola de emparejamiento, liberando CPU.

* `3) Sincronización de Movimientos`: Cada ficha corre de forma asíncrona, pero las actualizaciones al tablero se hacen secuencialmente por el hilo Juez (`jugar()`). La ficha propone un movimiento mediante una cola de `solicitudes` protegiendo el acceso con el mutex de la mesa, delegando la modificación del estado global al juez para evitar condiciones de carrera o estados inconsistentes.

* `4) Manejo Limpio de Señales`: Se utilizó la técnica de bloqueo de señales (`pthread_sigmask`) en el hilo principal antes de crear los demás hilos. De este modo, solo un hilo especializado y dedicado se queda bloqueado en `sigwait` escuchando `SIGINT`. Cuando se recibe, modifica una bandera atómica (`detener`) y despierta a todos los hilos en espera mediante un *broadcast*, asegurando una salida rápida pero estructurada.