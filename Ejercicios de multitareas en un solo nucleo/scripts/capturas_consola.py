"""
Convierte los logs de consola de la simulacion en imagenes tipo terminal.

El texto NO se retoca: se toma tal cual del fichero .log que produjo el
ejecutable. Lo unico que anade este script es el color por nivel de log
(I verde, W amarillo, E rojo), igual que hace el monitor serie de ESP-IDF,
para que la captura se lea como lo que es: la salida del programa corriendo.

Uso:  python scripts/capturas_consola.py
"""
import io
import os
import re

from PIL import Image, ImageDraw, ImageFont

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SALIDAS = os.path.join(RAIZ, "simulacion", "salidas")
FIGURAS = os.path.join(RAIZ, "docs", "evidencias")

FUENTE = r"C:\Windows\Fonts\consola.ttf"
FUENTE_B = r"C:\Windows\Fonts\consolab.ttf"
TAM = 15
INTERLINEA = 21
MARGEN = 18
BARRA = 34

FONDO = "#12120f"
BARRA_BG = "#26261f"
TITULO_FG = "#e8e6dd"
NEUTRO = "#c3c2b7"
SEPARADOR = "#3a3a31"

COLOR_NIVEL = {
    "I": "#5fd65f",   # info
    "W": "#f0b429",   # warning
    "E": "#ef5f5f",   # error
}

RE_LINEA = re.compile(r"^([IWE]) \((\d+)\)")


def render(lineas, destino, titulo):
    fuente = ImageFont.truetype(FUENTE, TAM)
    fuente_b = ImageFont.truetype(FUENTE_B, TAM)

    ancho_car = fuente.getlength("M")
    columnas = max((len(l) for l in lineas), default=80)
    ancho = int(MARGEN * 2 + ancho_car * (columnas + 1))
    alto = BARRA + MARGEN * 2 + INTERLINEA * len(lineas)

    img = Image.new("RGB", (ancho, alto), FONDO)
    d = ImageDraw.Draw(img)

    d.rectangle([0, 0, ancho, BARRA], fill=BARRA_BG)
    for i, c in enumerate(("#ef5f5f", "#f0b429", "#5fd65f")):
        d.ellipse([14 + i * 18, 12, 24 + i * 18, 22], fill=c)
    d.text((78, BARRA / 2), titulo, font=fuente_b, fill=TITULO_FG, anchor="lm")
    d.line([0, BARRA, ancho, BARRA], fill=SEPARADOR, width=1)

    y = BARRA + MARGEN
    for linea in lineas:
        m = RE_LINEA.match(linea)
        color = COLOR_NIVEL.get(m.group(1), NEUTRO) if m else NEUTRO
        f = fuente_b if (linea.startswith("=") or linea.startswith(" ")) else fuente
        d.text((MARGEN, y), linea, font=f, fill=color)
        y += INTERLINEA

    os.makedirs(FIGURAS, exist_ok=True)
    ruta = os.path.join(FIGURAS, destino)
    img.save(ruta)
    print("captura:", os.path.relpath(ruta, RAIZ), "({}x{})".format(ancho, alto))


def leer(ejemplo):
    ruta = os.path.join(SALIDAS, "ej{}_consola.log".format(ejemplo))
    with io.open(ruta, encoding="utf-8", errors="replace") as f:
        return [l.rstrip("\n").rstrip() for l in f]


def recorte(lineas, desde, hasta, marca_corte=True):
    """Toma dos tramos del log y los une con una marca de continuidad."""
    trozo = lineas[desde[0]:desde[1]]
    if hasta is not None:
        if marca_corte:
            trozo = trozo + ["", "        [...]", ""]
        trozo = trozo + lineas[hasta[0]:hasta[1]]
    return trozo


def buscar(lineas, texto, desde=0):
    for i in range(desde, len(lineas)):
        if texto in lineas[i]:
            return i
    raise ValueError("no encontrado: " + texto)


def main():
    # --- Ejemplo 1: cabecera + varios ciclos completos ---------------------
    l1 = leer(1)
    render(recorte(l1, (0, 20), None), "cap1_ej1_consola.png",
           "Ejemplo 1 - concurrencia de tres tareas   (salida real del simulador)")

    # --- Ejemplo 2: los siete envios de la cola ----------------------------
    l2 = leer(2)
    render(l2[0:22], "cap2_ej2_consola.png",
           "Ejemplo 2 - cola productor / consumidor   (salida real del simulador)")

    # --- Ejemplo 3: transicion a la fase B, watchdog y recuperacion --------
    l3 = leer(3)
    i_fase_b = buscar(l3, "FASE B")
    i_twdt = buscar(l3, "Task watchdog got triggered")
    i_rec = buscar(l3, "FASE C")
    # Los dos tramos no deben solaparse, o la captura repetiria lineas.
    fin_primero = min(i_twdt + 6, i_rec)
    render(recorte(l3, (i_fase_b - 4, fin_primero), (i_rec, i_rec + 7)),
           "cap3_ej3_watchdog.png",
           "Ejemplo 3 - inanicion y disparo del Task Watchdog Timer   (salida real)")

    # --- Ejemplo 4: latencias de preemcion ---------------------------------
    l4 = leer(4)
    i = buscar(l4, "Evento critico #1")
    render(l4[0:9] + ["", "        [...]", ""] + l4[i:i + 5],
           "cap4_ej4_preemcion.png",
           "Ejemplo 4 - preemcion por prioridad   (salida real del simulador)")

    # --- Ejemplo 5: reparto 50/50 ------------------------------------------
    l5 = leer(5)
    render(l5[0:20], "cap5_ej5_turnos.png",
           "Ejemplo 5 - reparto por turnos entre tareas de igual prioridad   (salida real)")


if __name__ == "__main__":
    main()
