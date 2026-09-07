#!/usr/bin/env bash
#
# Ejecuta los cinco experimentos y deja en salidas/ el log de consola y las
# trazas CSV de cada uno.
#
set -euo pipefail

AQUI="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXE="$AQUI/build/simulador.exe"
SALIDAS="$AQUI/salidas"

if [ ! -x "$EXE" ]; then
    echo "No existe $EXE. Ejecute primero ./compilar.sh" >&2
    exit 1
fi

mkdir -p "$SALIDAS"
rm -f "$SALIDAS"/*.log "$SALIDAS"/*.csv

# Duracion de cada experimento, en segundos de tiempo simulado.
#   ej3 necesita 20 s para cubrir las tres fases (5 s sanos + 8 s de bloqueo
#   + recuperacion). El resto solo necesita varios periodos completos.
declare -A DURACION=( [1]=12 [2]=13 [3]=20 [4]=11 [5]=6 )

for n in 1 2 3 4 5; do
    t="${DURACION[$n]}"
    printf 'Ejemplo %d (%s s de tiempo simulado)... ' "$n" "$t"
    "$EXE" "$n" "$t" "$SALIDAS" > "$SALIDAS/ej${n}_consola.log" 2>&1
    printf 'OK\n'
done

echo
echo "Resultados en: $SALIDAS"
ls -1 "$SALIDAS"
