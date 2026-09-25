"""
captura_monitor.py - Convierte los registros del monitor serie de la placa en
la imagen docs/evidencias/captura_monitor_serie.png.

El texto es literal: se toma tal cual de los ficheros capturados en COM5, sin
retocar ninguna linea. Solo se colorean las lineas por tipo de evento y, como
el registro de pulsaciones es largo, se muestra un extracto que indica de forma
explicita cuantas lineas se omiten.

    docs/evidencias/monitor_arranque.txt      arranque tras un reinicio
    docs/evidencias/monitor_pulsaciones.txt   pruebas P3 a P6

Uso:   python scripts/captura_monitor.py
"""
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

RAIZ = Path(__file__).resolve().parent.parent
EVID = RAIZ / "docs" / "evidencias"

FONDO = "#1E1E1E"
TEXTO = "#D4D4D4"
TENUE = "#808080"
VERDE = "#89D185"
GRIS_SUELTO = "#9CA3AF"
AMARILLO = "#E5C07B"

PRIMERAS = 10   # lineas de eventos al principio del extracto
ULTIMAS = 6     # y al final


def leer(nombre):
    texto = (EVID / nombre).read_text(encoding="utf-8", errors="replace")
    return [l.rstrip("\r") for l in texto.splitlines()]


def color(linea):
    if "PRESIONADO" in linea:
        return VERDE
    if "SUELTO" in linea:
        return GRIS_SUELTO
    if "pull-down: ACTIVADO" in linea or linea.startswith("Listo"):
        return AMARILLO
    if linea.startswith("ESP-ROM") or linea.startswith(("Build:", "rst:", "SPIWP", "mode:",
                                                       "load:", "entry")):
        return TENUE
    return TEXTO


def main():
    arranque = [l for l in leer("monitor_arranque.txt")]
    eventos = [l for l in leer("monitor_pulsaciones.txt") if l.startswith("[")]

    lineas = [("--- Arranque tras un reinicio (monitor_arranque.txt) ---", TENUE)]
    lineas += [(l, color(l)) for l in arranque if l.strip()]
    lineas.append(("", TEXTO))
    lineas.append(("--- Pruebas P3 a P6 (monitor_pulsaciones.txt, extracto) ---", TENUE))
    lineas += [(l, color(l)) for l in eventos[:PRIMERAS]]
    omitidas = len(eventos) - PRIMERAS - ULTIMAS
    lineas.append((f"      [ ... {omitidas} lineas omitidas en esta imagen; "
                   f"registro completo en monitor_pulsaciones.txt ... ]", AMARILLO))
    lineas += [(l, color(l)) for l in eventos[-ULTIMAS:]]

    ancho_car = max(len(l) for l, _ in lineas)
    alto_linea = 0.17
    fig = plt.figure(figsize=(0.075 * ancho_car + 0.4, alto_linea * len(lineas) + 0.4))
    fig.patch.set_facecolor(FONDO)
    ax = fig.add_axes([0, 0, 1, 1])
    ax.set_facecolor(FONDO)
    ax.axis("off")
    ax.set_xlim(0, 1)
    ax.set_ylim(len(lineas) + 0.6, -0.6)
    for i, (l, c) in enumerate(lineas):
        ax.text(0.012, i, l, family="DejaVu Sans Mono", fontsize=8.6, color=c, va="center")

    salida = EVID / "captura_monitor_serie.png"
    fig.savefig(salida, dpi=170, facecolor=FONDO)
    plt.close(fig)
    print("  ", salida.relative_to(RAIZ), f"({len(lineas)} lineas, {omitidas} omitidas)")


if __name__ == "__main__":
    main()
