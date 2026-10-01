"""
generar_figuras.py - Figuras explicativas del informe (no contienen datos medidos).

    fig1_arquitectura.png   Quien publica y quien se suscribe a cada topico

Las graficas con datos reales de la placa las genera graficas_placa.py.

Uso:   python scripts/generar_figuras.py
Requiere:  pip install matplotlib
"""
import matplotlib.pyplot as plt
from matplotlib.patches import FancyArrowPatch, FancyBboxPatch

from figuras_comun import EJE, SERIE_1, SUPERFICIE, TINTA, TINTA_2, TENUE, guardar

FONDO_BROKER = "#e8f0fb"     # paso claro de la rampa azul: el broker es el centro
PASO = 2.2                   # interlineado dentro de las cajas (1 unidad = 0,1 pulgadas)


def caja(ax, x, y, w, h, titulo, lineas, fondo=SUPERFICIE, borde=EJE):
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0,rounding_size=1.2",
                                fc=fondo, ec=borde, lw=1.0))
    ax.text(x + w / 2, y + h - 2.0, titulo, ha="center", va="top", fontsize=11,
            fontweight="bold", color=TINTA)
    for i, l in enumerate(lineas):
        ax.text(x + w / 2, y + h - 5.6 - i * PASO, l, ha="center", va="top", fontsize=9, color=TINTA_2)


def flecha(ax, origen, destino):
    ax.add_patch(FancyArrowPatch(origen, destino, arrowstyle="-|>", mutation_scale=13,
                                 lw=1.6, color=SERIE_1, shrinkA=1, shrinkB=1))


def nota(ax, x, y, lineas, va="bottom", mono=False):
    """Texto junto a una flecha. En las notas de topico, la primera linea (el
    topico) va en tinta y la segunda (el mensaje) en gris."""
    n = len(lineas)
    for i, l in enumerate(lineas):
        yi = y - i * PASO if va == "top" else y + (n - 1 - i) * PASO
        ax.text(x, yi, l, ha="center", va=va, fontsize=8.6 if mono else 8.4,
                color=TINTA if (mono and i == 0) else TINTA_2, family="Consolas" if mono else None)


def fig_arquitectura():
    fig = plt.figure(figsize=(11, 5.2))
    ax = fig.add_axes([0, 0, 1, 1])
    ax.set_xlim(0, 110)
    ax.set_ylim(0, 52)
    ax.axis("off")

    caja(ax, 1, 33, 22, 15, "ESP32-C6 · publicador",
         ["sensor de temperatura", "cliente esp-mqtt", "publica cada 20 s"])
    caja(ax, 1, 6, 22, 15, "ESP32-C6 · suscriptor",
         ["cliente esp-mqtt", "MQTT_EVENT_DATA", "field1 = 1 / 0 → LED"])
    caja(ax, 44, 16, 22, 18, "Broker MQTT",
         ["broker.hivemq.com", "TCP 1883 · WebSocket 8884", "en lugar de", "mqtt3.thingspeak.com"],
         fondo=FONDO_BROKER, borde=SERIE_1)
    caja(ax, 87, 18, 22, 14, "Tablero web",
         ["navegador · MQTT.js", "dibuja la temperatura", "envía comandos al LED"])

    # Temperatura: publicador -> broker -> tablero
    flecha(ax, (23, 40), (44, 30))
    nota(ax, 33.5, 39.6, ["channels/<canal>/publish", "field1=29.80&status=..."], va="bottom", mono=True)
    flecha(ax, (66, 29), (87, 29))
    nota(ax, 76.5, 29.8, ["entrega a quien", "esté suscrito"], va="bottom")

    # Comandos: tablero -> broker -> suscriptor
    flecha(ax, (87, 21), (66, 21))
    nota(ax, 76.5, 19.6, ["channels/<canal>/subscribe", '{"field1":"1", ...}'], va="top", mono=True)
    flecha(ax, (44, 20), (23, 13))
    nota(ax, 33.5, 12.4, ["entrega inmediata", "(sin polling)"], va="top")

    ax.text(55, 1.4, "Publicadores y suscriptores no se conocen: solo comparten el nombre del tópico.",
            ha="center", va="bottom", fontsize=9, color=TENUE, style="italic")
    guardar(fig, "fig1_arquitectura.png")


if __name__ == "__main__":
    fig_arquitectura()
