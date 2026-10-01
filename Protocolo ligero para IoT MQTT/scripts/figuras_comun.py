"""
figuras_comun.py - Estilo y lectura de registros, comunes a las figuras del informe.

Paleta y reglas de la guia de visualizacion de datos que siguen los informes
anteriores del repositorio: marcas finas, rejilla discreta, texto siempre con
tinta (nunca con el color de la serie) y una sola serie en azul.

LOS DOS REGISTROS COMPARTEN RELOJ
---------------------------------
scripts/capturar_monitor.py (salida serie de la placa) y scripts/cliente_mqtt.js
(cliente MQTT del PC) sellan cada linea con la hora UTC del mismo PC, en
milisegundos. Por eso se pueden restar para medir cuanto tarda un mensaje en
ir de la placa al PC a traves del broker, o al reves. La hora que imprime
ESP_LOG, "I (21102)", es otro reloj: los milisegundos desde el arranque del
chip. Sirve para medir intervalos dentro de la placa, sin el retardo del USB.
"""
import re
from datetime import datetime
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

RAIZ = Path(__file__).resolve().parent.parent
EVID = RAIZ / "docs" / "evidencias"
EVID.mkdir(parents=True, exist_ok=True)

# --- Paleta (modo claro) ---------------------------------------------------
SUPERFICIE = "#fcfcfb"
PAGINA = "#ffffff"
TINTA = "#0b0b0b"
TINTA_2 = "#52514e"
TENUE = "#898781"
REJILLA = "#e1e0d9"
EJE = "#c3c2b7"
SERIE_1 = "#2a78d6"
LAVADO_1 = "#2a78d61a"       # la serie al 10 %, para areas
DPI = 200

plt.rcParams.update({
    "font.family": ["Segoe UI", "DejaVu Sans", "sans-serif"],
    "font.size": 10,
    "figure.facecolor": PAGINA,
    "axes.facecolor": SUPERFICIE,
    "axes.edgecolor": EJE,
    "axes.labelcolor": TINTA_2,
    "axes.titlecolor": TINTA,
    "axes.linewidth": 0.8,
    "axes.grid": True,
    "grid.color": REJILLA,
    "grid.linewidth": 0.7,
    "grid.linestyle": "-",
    "xtick.color": TENUE,
    "ytick.color": TENUE,
    "xtick.labelcolor": TINTA_2,
    "ytick.labelcolor": TINTA_2,
    "legend.frameon": False,
})


def estilo(ax):
    """Sin marco arriba ni a la derecha; rejilla por debajo de los datos."""
    for lado in ("top", "right"):
        ax.spines[lado].set_visible(False)
    ax.tick_params(length=0)
    ax.set_axisbelow(True)


def guardar(fig, nombre):
    ruta = EVID / nombre
    fig.savefig(ruta, dpi=DPI, bbox_inches="tight", facecolor=PAGINA)
    plt.close(fig)
    print("figura:", ruta.relative_to(RAIZ))


# --- Lectura de registros -------------------------------------------------

LINEA_SERIE = re.compile(r"^\[(?P<iso>\S+Z) \|\s*(?P<rel>[\d.]+) s\] (?P<texto>.*)$")
# El espacio tras ":" es opcional: el Wi-Fi escribe "wifi:connected with ..."
LINEA_ESP = re.compile(r"^(?P<nivel>[IWE]) \((?P<ms>\d+)\) (?P<tag>[^:]+):\s?(?P<msg>.*)$")
LINEA_PC = re.compile(r"^\[(?P<iso>\S+Z)\] \+[\d.]+ s  (?P<texto>.*)$")


def hora(iso):
    """'2026-10-01T03:31:43.923Z' -> datetime con zona horaria (UTC)."""
    return datetime.fromisoformat(iso.replace("Z", "+00:00"))


def leer_serie(nombre):
    """Lineas de capturar_monitor.py: (hora_pc, ms_chip o None, tag, mensaje)."""
    filas = []
    for linea in (EVID / nombre).read_text(encoding="utf-8").splitlines():
        m = LINEA_SERIE.match(linea)
        if not m:
            continue
        texto = m["texto"]
        e = LINEA_ESP.match(texto)
        if e:
            filas.append((hora(m["iso"]), int(e["ms"]), e["tag"].strip(), e["msg"]))
        else:
            filas.append((hora(m["iso"]), None, "", texto))
    return filas


def leer_pc(nombre):
    """Lineas de cliente_mqtt.js: (hora_pc, texto)."""
    filas = []
    for linea in (EVID / nombre).read_text(encoding="utf-8").splitlines():
        m = LINEA_PC.match(linea)
        if m:
            filas.append((hora(m["iso"]), m["texto"]))
    return filas
