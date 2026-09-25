#!/usr/bin/env bash
# Compila el sketch contra el Arduino simulado y ejecuta la prueba de logica.
# La salida se guarda en pruebas/salida_simulacion_pc.txt.
#
# Requiere g++ (en Windows, MinGW-w64). Otro compilador: GXX=/ruta/g++ bash pruebas/ejecutar.sh
set -e
cd "$(dirname "$0")"
"${GXX:-g++}" -std=c++17 -Wall -Wextra -I simulado simulacion_pc.cpp -o simulacion_pc.exe
./simulacion_pc.exe | tee salida_simulacion_pc.txt
