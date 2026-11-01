CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -pthread

FUENTES = main.cpp configuracion.cpp tablero.cpp partida.cpp servidor.cpp

all: torneo

torneo: $(FUENTES) komikan.h
	$(CXX) $(CXXFLAGS) $(FUENTES) -o torneo

clean:
	rm -f torneo

.PHONY: all clean test

test:
	$(CXX) $(CXXFLAGS) pruebas_unitarias.cpp tablero.cpp -o /tmp/pruebas_unitarias
	/tmp/pruebas_unitarias
