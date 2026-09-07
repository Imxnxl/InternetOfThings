#!/usr/bin/env bash
#
# Compila la simulacion enlazando el kernel real de FreeRTOS (port MSVC-MingW).
#
# Requisitos (ver README del repositorio):
#   - MinGW-w64 en  tools/mingw64
#   - FreeRTOS-Kernel en  tools/FreeRTOS-Kernel
#
set -euo pipefail

AQUI="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RAIZ="$(cd "$AQUI/.." && pwd)"

# Busca la carpeta tools/ subiendo desde aqui. Se permite que este fuera del
# trabajo (por ejemplo, en la raiz del repositorio) para poder compartirla
# entre varios trabajos y, sobre todo, para poder instalarla en una ruta SIN
# ESPACIOS: MinGW no funciona si su propia ruta de instalacion los contiene
# (ld.exe la parte por el espacio y no encuentra sus librerias).
buscar_tools() {
    local dir="$RAIZ"
    for _ in 1 2 3 4; do
        if [ -d "$dir/tools" ]; then
            echo "$dir/tools"
            return 0
        fi
        dir="$(cd "$dir/.." && pwd)"
    done
    echo "$RAIZ/tools"
}

TOOLS="${TOOLS:-$(buscar_tools)}"
GCC="${GCC:-$TOOLS/mingw64/bin/gcc.exe}"
KERNEL="${KERNEL:-$TOOLS/FreeRTOS-Kernel}"
COMPARTIDO="$RAIZ/shared/ejemplos_freertos/src"
SALIDA="$AQUI/build"

case "$GCC" in
    *" "*) echo "AVISO: la ruta del compilador contiene espacios; MinGW fallara al enlazar." >&2 ;;
esac

for req in "$GCC" "$KERNEL/tasks.c" "$KERNEL/portable/MSVC-MingW/port.c"; do
    if [ ! -e "$req" ]; then
        echo "ERROR: no se encuentra $req" >&2
        echo "Ejecute primero  scripts/preparar_entorno.sh" >&2
        exit 1
    fi
done

mkdir -p "$SALIDA"

FUENTES=(
    # --- Kernel real de FreeRTOS ---
    "$KERNEL/tasks.c"
    "$KERNEL/queue.c"
    "$KERNEL/list.c"
    "$KERNEL/timers.c"
    "$KERNEL/event_groups.c"
    "$KERNEL/stream_buffer.c"
    "$KERNEL/portable/MSVC-MingW/port.c"
    "$KERNEL/portable/MemMang/heap_4.c"
    # --- Capa de simulacion ---
    "$AQUI/main_host.c"
    "$AQUI/shim/plataforma_host.c"
    "$AQUI/shim/traza.c"
    # --- Ejemplos (codigo compartido con el firmware del ESP32-C6) ---
    "$COMPARTIDO/ej1_time_slicing.c"
    "$COMPARTIDO/ej2_cola.c"
    "$COMPARTIDO/ej3_starvation_wdt.c"
    "$COMPARTIDO/ej4_prioridades.c"
    "$COMPARTIDO/ej5_reparto_turnos.c"
)

INCLUDES=(
    -I"$AQUI"                              # FreeRTOSConfig.h
    -I"$AQUI/shim"                         # puente freertos/*.h, traza.h
    -I"$KERNEL/include"
    -I"$KERNEL/portable/MSVC-MingW"
    -I"$COMPARTIDO"                        # plataforma.h, ejemplos.h
)

echo "Compilando con: $("$GCC" --version | head -1)"
echo "Kernel FreeRTOS: $KERNEL"

"$GCC" -std=c11 -O1 -g -Wall -Wextra -Wno-unused-parameter \
    "${INCLUDES[@]}" \
    "${FUENTES[@]}" \
    -o "$SALIDA/simulador.exe" \
    -lwinmm

echo "OK -> $SALIDA/simulador.exe"
