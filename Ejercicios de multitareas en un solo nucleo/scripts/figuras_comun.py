"""
Utilidades comunes para generar las figuras del informe.

Paleta y reglas de estilo tomadas de la guia de visualizacion de datos:
categoricos asignados por identidad (nunca por rango ni cicladas), marcas
finas, rejilla discreta, y colores de estado reservados para lo que de verdad
es un estado (el disparo del watchdog).

DOS RELOJES, QUE NO HAY QUE MEZCLAR
-----------------------------------
La traza guarda cada cambio de contexto con dos sellos de tiempo:

  tick_ms : tiempo SIMULADO del microcontrolador (el tick de FreeRTOS).
            Exacto, con resolucion de 1 ms. Es el unico reloj que representa
            lo que veria un ESP32-C6: un vTaskDelay(500) son 500 ticks aqui
            y 500 ticks en el chip.

  us      : reloj de pared del PC, en microsegundos. Sirve para ordenar lo que
            ocurre DENTRO de un mismo tick y para ver que rodaja se corta
            antes de tiempo. Su duracion absoluta es la del PC y NO representa
            la del ESP32-C6: en las figuras solo se usa para mostrar el orden
            de los sucesos en las vistas de detalle, nunca como medida.
"""
import csv
import os

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Patch

# --- Paleta (modo claro) ---------------------------------------------------
SURFACE = "#fcfcfb"
PAGE    = "#f9f9f7"
INK     = "#0b0b0b"
INK_2   = "#52514e"
MUTED   = "#898781"
GRID    = "#e1e0d9"
AXIS    = "#c3c2b7"

SERIE = ["#2a78d6", "#eb6834", "#1baf7a", "#eda100"]   # slots categoricos 1..4
CRITICAL = "#d03b3b"
WARNING  = "#fab219"
GOOD     = "#0ca30c"

# El Idle no es una serie: es la ausencia de trabajo. Va en gris para que
# retroceda visualmente y no compita con las tareas reales.
COLOR_IDLE  = "#d8d7d0"
COLOR_INFRA = "#bfbeb6"   # tareas propias del simulador (Supervisor, Tmr Svc)

INFRA = {"Supervisor", "Tmr Svc"}

plt.rcParams.update({
    "font.family":      ["Segoe UI", "DejaVu Sans", "sans-serif"],
    "font.size":        10,
    "figure.facecolor": PAGE,
    "axes.facecolor":   SURFACE,
    "axes.edgecolor":   AXIS,
    "axes.labelcolor":  INK_2,
    "axes.titlecolor":  INK,
    "axes.linewidth":   0.8,
    "axes.grid":        True,
    "grid.color":       GRID,
    "grid.linewidth":   0.7,
    "grid.linestyle":   "-",
    "xtick.color":      MUTED,
    "ytick.color":      MUTED,
    "xtick.labelcolor": INK_2,
    "ytick.labelcolor": INK_2,
    "legend.frameon":   False,
    "figure.dpi":       130,
})

RAIZ    = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SALIDAS = os.path.join(RAIZ, "simulacion", "salidas")
FIGURAS = os.path.join(RAIZ, "docs", "evidencias")


def leer_csv(nombre):
    with open(os.path.join(SALIDAS, nombre), newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def cargar_contexto(ejemplo):
    """Rodajas de CPU: nombre, tick simulado y ventana en us de pared."""
    filas = leer_csv("ej{}_contexto.csv".format(ejemplo))
    return [{
        "tarea": f["tarea"],
        "tick":  float(f["tick_ms"]),
        "us0":   float(f["us_inicio"]),
        "us1":   float(f["us_fin"]),
        "dur":   float(f["duracion_us"]),
    } for f in filas]


def clave_fila(nombre):
    if nombre.startswith("IDLE"):
        return "IDLE"
    if nombre in INFRA:
        return "infra"
    return nombre


def color_de(nombre, mapa):
    c = clave_fila(nombre)
    if c == "IDLE":
        return COLOR_IDLE
    if c == "infra":
        return COLOR_INFRA
    return mapa.get(nombre, COLOR_INFRA)


def etiqueta_fila(clave):
    if clave == "IDLE":
        return "IDLE  (nucleo libre)"
    if clave == "infra":
        return "infra. simulador"
    return clave


def _preparar_filas(ax, orden):
    ax.set_yticks(range(len(orden)))
    ax.set_yticklabels([etiqueta_fila(o) for o in orden])
    ax.set_ylim(len(orden) - 0.45, -0.55)
    ax.grid(axis="y", visible=False)
    ax.set_axisbelow(True)


def banda_eventos(ax, filas, orden, mapa, t0, t1, altura=0.52):
    """
    Vista larga: una marca vertical por cada vez que la tarea obtuvo el nucleo.

    En esta escala las rodajas duran microsegundos y serian invisibles, asi que
    se dibujan como SUCESOS (cuando corrio cada tarea), no como duraciones.
    Para la duracion real estan las vistas de detalle.
    """
    idx = {o: i for i, o in enumerate(orden)}
    for o in orden:
        ts = [f["tick"] for f in filas
              if clave_fila(f["tarea"]) == o and t0 <= f["tick"] <= t1]
        if not ts:
            continue
        color = (COLOR_IDLE if o == "IDLE" else
                 COLOR_INFRA if o == "infra" else mapa[o])
        ax.eventplot(ts, lineoffsets=idx[o], linelengths=altura,
                     linewidths=1.5, colors=color)
    _preparar_filas(ax, orden)
    ax.set_xlim(t0, t1)


def gantt_us(ax, filas, orden, mapa, us0, us1, altura=0.6):
    """
    Vista de detalle: rodajas reales, con el eje en microsegundos del
    simulador y el origen puesto en us0. Lo que se lee aqui es el ORDEN de
    los sucesos dentro de un tick, no una duracion trasladable al ESP32-C6.
    """
    idx = {o: i for i, o in enumerate(orden)}
    for f in filas:
        if f["us1"] < us0 or f["us0"] > us1:
            continue
        c = clave_fila(f["tarea"])
        if c not in idx:
            continue
        a = max(f["us0"], us0) - us0
        b = min(f["us1"], us1) - us0
        # Borde del color del lienzo: separa rodajas contiguas de la misma
        # tarea, que si no se leerian como un unico bloque continuo.
        ax.broken_barh([(a, max(b - a, 0.5))],
                       (idx[c] - altura / 2, altura),
                       facecolors=color_de(f["tarea"], mapa),
                       edgecolor=SURFACE, linewidth=0.7)
    _preparar_filas(ax, orden)
    ax.set_xlim(0, us1 - us0)


def gantt_ticks(ax, filas, orden, mapa, t0, t1, altura=0.6):
    """Vista de detalle en ticks: util cuando cada rodaja dura un tick entero."""
    idx = {o: i for i, o in enumerate(orden)}
    for f in filas:
        c = clave_fila(f["tarea"])
        if c not in idx or not (t0 <= f["tick"] <= t1):
            continue
        ax.broken_barh([(f["tick"], 1.0)], (idx[c] - altura / 2, altura),
                       facecolors=color_de(f["tarea"], mapa),
                       edgecolor=SURFACE, linewidth=1.0)
    _preparar_filas(ax, orden)
    ax.set_xlim(t0, t1 + 1)


def manchas_leyenda(orden, mapa, extra=None):
    manchas = []
    for o in orden:
        c = (COLOR_IDLE if o == "IDLE" else
             COLOR_INFRA if o == "infra" else mapa[o])
        manchas.append(Patch(facecolor=c, label=etiqueta_fila(o)))
    if extra:
        manchas.extend(extra)
    return manchas


def leyenda(ax, orden, mapa, extra=None, y=-0.42, ncol=None):
    manchas = manchas_leyenda(orden, mapa, extra)
    ax.legend(handles=manchas, loc="upper center", bbox_to_anchor=(0.5, y),
              ncol=ncol or min(4, len(manchas)), fontsize=9, labelcolor=INK_2)


def leyenda_al_pie(fig, orden, mapa, extra=None, y=-0.02, ncol=None):
    """Leyenda comun al pie de toda la figura, para graficas de varios paneles."""
    manchas = manchas_leyenda(orden, mapa, extra)
    fig.legend(handles=manchas, loc="lower center", bbox_to_anchor=(0.5, y),
               ncol=ncol or min(6, len(manchas)), fontsize=9,
               labelcolor=INK_2, frameon=False)


def titulo(fig, texto, subtitulo=None):
    fig.text(0.012, 0.978, texto, fontsize=13, fontweight="bold",
             color=INK, va="top")
    if subtitulo:
        fig.text(0.012, 0.930, subtitulo, fontsize=9.5, color=INK_2, va="top")


def guardar(fig, nombre):
    os.makedirs(FIGURAS, exist_ok=True)
    ruta = os.path.join(FIGURAS, nombre)
    fig.savefig(ruta, facecolor=PAGE, bbox_inches="tight", pad_inches=0.22)
    plt.close(fig)
    print("figura:", os.path.relpath(ruta, RAIZ))
    return ruta
