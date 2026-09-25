"""
generar_figuras.py - Genera las figuras del informe en docs/evidencias/.

    fig1_circuito_clase.png            Circuito de clase: pull-down con resistencia fisica
    fig2_circuito_pulldown_interno.png Esta practica: pull-down interno del ESP32-C6
    fig3_conexiones.png                Diagrama de conexiones sobre la ESP32-C6-DevKitC-1

Los dos esquemas se dibujan con schemdraw y el diagrama de conexiones con
matplotlib. Son figuras explicativas: no contienen datos medidos.

Uso:   python scripts/generar_figuras.py
Requiere:  pip install schemdraw matplotlib
"""
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, Circle, Rectangle

import schemdraw
import schemdraw.elements as elm

schemdraw.use("matplotlib")

RAIZ = Path(__file__).resolve().parent.parent
SALIDA = RAIZ / "docs" / "evidencias"
SALIDA.mkdir(parents=True, exist_ok=True)

TINTA = "#1A1A1A"
TINTA_2 = "#52514E"
ACENTO = "#1C5CAB"      # lo que cambia respecto al circuito de clase
ROJO, VERDE, AZUL = "#C62828", "#2E7D32", "#1565C0"
DPI = 200


def _guardar(d, nombre):
    d.save(str(SALIDA / nombre), dpi=DPI)
    print("  ", nombre)


# ------------------------------------------------------------------ fig 1 --
def fig_circuito_clase():
    """Pull-down con resistencia fisica de 10 kohm, como en el ejercicio de clase."""
    with schemdraw.Drawing(show=False) as d:
        d.config(fontsize=11, unit=2.2, lw=1.4, color=TINTA)

        # Entrada: 3V3 -> pulsador -> nodo -> GPIO ; nodo -> 10k -> GND
        elm.Vdd().label("3V3")
        elm.Button().down().label("Pulsador", loc="bottom", ofst=0.25)
        nodo = elm.Dot()
        d.push()
        elm.Resistor().down().color(ACENTO).label(
            "R pull-down\n10 kΩ (física)", loc="bottom", ofst=0.25, color=ACENTO)
        elm.Ground()
        d.pop()
        elm.Line().right(d.unit * 1.3).at(nodo.center)
        ent = elm.Dot(open=True).label("GPIO\n(entrada)", loc="right", ofst=0.15)

        # Salida: GPIO -> 220 ohm -> LED -> GND
        x0 = ent.center[0] + d.unit * 2.2
        elm.Dot(open=True).at((x0, nodo.center[1])).label(
            "GPIO\n(salida)", loc="left", ofst=0.15)
        elm.Line().right(d.unit * 0.5)
        elm.Resistor().down().label("220 Ω", loc="bottom", ofst=0.25)
        elm.LED().down().label("LED", loc="bottom", ofst=0.25)
        elm.Ground()

        elm.Label().at((nodo.center[0] + d.unit * 1.9, nodo.center[1] + d.unit * 1.1)).label(
            "ESP32-C6: pinMode(pin, INPUT)", fontsize=10.5, color=TINTA_2)

        _guardar(d, "fig1_circuito_clase.png")


# ------------------------------------------------------------------ fig 2 --
def fig_circuito_interno():
    """Pull-down interno: la resistencia esta dentro del chip, tras un interruptor
    que se cierra por software con INPUT_PULLDOWN."""
    with schemdraw.Drawing(show=False) as d:
        d.config(fontsize=11, unit=2.2, lw=1.4, color=TINTA)

        # --- Entrada ------------------------------------------------------
        elm.Vdd().label("3V3")
        elm.Button().down().label("Pulsador", loc="bottom", ofst=0.25)
        pin = elm.Dot(open=True).label("GPIO18", loc="left", ofst=0.2)
        elm.Line().right(d.unit * 0.9).at(pin.center)
        nodo = elm.Dot()
        d.push()
        logica = elm.Line().right(d.unit * 1.1).label("digitalRead()", loc="top", ofst=0.12,
                                                      fontsize=10, color=TINTA_2)
        d.pop()
        sw = elm.Switch(action="close").down().at(nodo.center).color(ACENTO).label(
            "INPUT_PULLDOWN", loc="bottom", ofst=0.3, color=ACENTO)
        rpd = elm.Resistor().down().color(ACENTO).label(
            "R_PD ≈ 45 kΩ", loc="bottom", ofst=0.3, color=ACENTO)
        gnd = elm.Ground().color(ACENTO)
        elm.EncircleBox([nodo, logica, sw, rpd, gnd], padx=0.55, pady=0.45).linestyle("--").color(
            TINTA_2).label("dentro del ESP32-C6", loc="bottom", fontsize=10, color=TINTA_2)

        # --- Salida: LED RGB de catodo comun --------------------------------
        y_pin = pin.center[1]
        x_pin = nodo.center[0] + d.unit * 3.1
        leds = []
        for i, (gpio, canal, color) in enumerate(
                [("GPIO19", "R", ROJO), ("GPIO20", "G", VERDE), ("GPIO21", "B", AZUL)]):
            x = x_pin + i * d.unit * 0.95
            elm.Dot(open=True).at((x, y_pin + d.unit * 0.45)).label(gpio, loc="top", ofst=0.1,
                                                                      fontsize=10)
            led = elm.LED().down().color(color).label(canal, loc="bottom", ofst=0.2, color=color)
            leds.append(led)
        # Catodo comun
        elm.Line().at(leds[0].end).to(leds[2].end)
        elm.Dot().at(leds[1].end)
        elm.Line().down(d.unit * 0.35).at(leds[1].end)
        elm.Ground()
        elm.Label().at((leds[1].end[0], leds[1].end[1] - d.unit * 1.05)).label(
            "cátodo común (pata larga)", fontsize=9.5, color=TINTA_2)
        elm.Label().at((leds[1].end[0], y_pin + d.unit * 1.25)).label(
            "sin resistencia en serie:\nfuerza de salida reducida (GPIO_DRIVE_CAP_0)",
            fontsize=9.5, color=ACENTO)

        _guardar(d, "fig2_circuito_pulldown_interno.png")


# ------------------------------------------------------------------ fig 3 --
# Conectores de la ESP32-C6-DevKitC-1, de arriba (lado antena) a abajo (lado USB).
J1 = ["3V3", "RST", "4", "5", "6", "7", "0", "1", "8", "10", "11", "2", "3", "5V", "G", "NC"]
J3 = ["G", "TX", "RX", "15", "23", "22", "21", "20", "19", "18", "9", "G", "13", "12", "G", "NC"]
EVITAR = {"4", "5", "8", "9", "15", "12", "13", "TX", "RX"}   # arranque, USB y UART

# (conector, indice) -> (texto de la conexion, color)
USADOS = {
    ("J1", 0): ("3V3  →  pulsador (pata 2)", "#E53935"),
    ("J3", 6): ("GPIO21  →  LED, pata B", AZUL),
    ("J3", 7): ("GPIO20  →  LED, pata G", VERDE),
    ("J3", 8): ("GPIO19  →  LED, pata R", ROJO),
    ("J3", 9): ("GPIO18  →  pulsador (pata 1)", "#E0A100"),
    ("J3", 11): ("GND  →  LED, pata común (la larga)", "#212121"),
}


def fig_conexiones():
    fig, ax = plt.subplots(figsize=(12.5, 8.8))
    ax.set_xlim(-2.8, 21.0)
    ax.set_ylim(-2.3, 17.6)
    ax.set_aspect("equal")
    ax.axis("off")

    xL, xR, y_top, paso = 3.2, 8.4, 15.9, 0.92

    # --- Placa --------------------------------------------------------------
    ax.add_patch(FancyBboxPatch((xL - 0.6, -0.45), (xR - xL) + 1.2, 17.3,
                                boxstyle="round,pad=0.02,rounding_size=0.25",
                                fc="#23282E", ec="#101316", lw=1.5, zorder=1))
    ax.add_patch(Rectangle((xL + 1.35, 12.0), xR - xL - 2.7, 4.2, fc="#C9CDD2", ec="#8A9096",
                           zorder=2))
    ax.text((xL + xR) / 2, 14.1, "módulo\nESP32-C6\nWROOM-1", ha="center", va="center",
            fontsize=8.5, color="#2B2F33", zorder=3)
    ax.text((xL + xR) / 2, 16.45, "antena", ha="center", va="center", fontsize=7.5,
            color="#C9CDD2", zorder=3)
    for xu, nombre in [(xL + 0.9, "USB"), (xR - 1.9, "UART")]:
        ax.add_patch(Rectangle((xu, -0.8), 1.0, 0.75, fc="#B0B5BA", ec="#6D7378", zorder=2))
        ax.text(xu + 0.5, -0.95, nombre, ha="center", va="top", fontsize=8, color=TINTA_2)
    ax.text((xL + xR) / 2, 6.2, "ESP32-C6\nDevKitC-1\n\nvista superior", ha="center",
            va="center", fontsize=9, color="#DADDE0", zorder=3)

    # --- Pines y anotaciones --------------------------------------------------
    for lado, pines, x in [("J1", J1, xL), ("J3", J3, xR)]:
        ax.text(x, y_top + 0.62, lado, ha="center", fontsize=8.5, color="#DADDE0",
                fontweight="bold", zorder=3)
        for i, p in enumerate(pines):
            y = y_top - i * paso
            uso = USADOS.get((lado, i))
            if uso:
                texto, color = uso
                ax.add_patch(Circle((x, y), 0.26, fc=color, ec="white", lw=1.6, zorder=5))
                # linea de anotacion hacia fuera de la placa
                if lado == "J1":
                    x_txt = x - 1.6
                    ax.plot([x - 0.26, x_txt + 0.1], [y, y], color=color, lw=2.4, zorder=4)
                    ax.text(x_txt, y, texto, ha="right", va="center", fontsize=9.5,
                            color=TINTA, fontweight="bold")
                else:
                    x_txt = x + 1.6
                    ax.plot([x + 0.26, x_txt - 0.1], [y, y], color=color, lw=2.4, zorder=4)
                    ax.text(x_txt, y, texto, ha="left", va="center", fontsize=9.5,
                            color=TINTA, fontweight="bold")
            else:
                evitar = p in EVITAR
                ax.add_patch(Circle((x, y), 0.19, fc="#5E5540" if evitar else "#C8A951",
                                    ec="#3A3426", lw=0.8, zorder=4))
            # etiqueta serigrafiada, por dentro de la placa
            dx, ha = (0.4, "left") if lado == "J1" else (-0.4, "right")
            ax.text(x + dx, y, p, ha=ha, va="center", fontsize=7.8,
                    color="#8C9196" if (p in EVITAR and not uso) else "#F2F2F2",
                    fontweight="bold" if uso else "normal", zorder=4)

    # --- Componentes ----------------------------------------------------------
    # LED RGB de 5 mm, patas hacia abajo, orden fisico R - comun - G - B
    lx, ly = 18.3, 10.4
    ax.add_patch(Rectangle((lx - 0.95, ly), 1.9, 1.5, fc="#F7F7F7", ec="#A0A0A0", lw=1.2,
                           zorder=3))
    ax.add_patch(matplotlib.patches.Wedge((lx, ly + 1.5), 0.95, 0, 180, fc="#F7F7F7",
                                          ec="#A0A0A0", lw=1.2, zorder=3))
    ax.text(lx, ly + 0.8, "LED RGB", ha="center", va="center", fontsize=9, color=TINTA,
            zorder=4)
    patas = [("R", ROJO, -0.72, 2.0), ("común", "#212121", -0.24, 2.7),
             ("G", VERDE, 0.24, 2.0), ("B", AZUL, 0.72, 2.0)]
    for nombre, color, dxp, largo in patas:
        x = lx + dxp
        ax.plot([x, x], [ly, ly - largo], color="#9E9E9E", lw=2.4, zorder=3,
                solid_capstyle="butt")
        ax.text(x, ly - largo - 0.15, nombre, ha="center", va="top", fontsize=8.5,
                color=color, fontweight="bold", rotation=90 if nombre == "común" else 0)
    ax.text(lx, ly + 2.8, "Pata más larga = común", ha="center", fontsize=8.5,
            color=TINTA_2)

    # Pulsador de 4 patas (las patas enfrentadas por el lado largo ya estan unidas
    # por dentro; usando dos patas en diagonal se acierta siempre).
    bx, by = 18.3, 3.3
    ax.add_patch(Rectangle((bx - 1.0, by - 1.0), 2.0, 2.0, fc="#37474F", ec="#263238",
                           lw=1.2, zorder=3))
    ax.add_patch(Circle((bx, by), 0.55, fc="#90A4AE", ec="#263238", zorder=4))
    esquinas = [(-1.0, 0.7), (1.0, 0.7), (-1.0, -0.7), (1.0, -0.7)]
    for k, (dx, dy) in enumerate(esquinas):
        x0 = bx + dx
        x1 = x0 + (-0.45 if dx < 0 else 0.45)
        diagonal = k in (0, 3)
        ax.plot([x0, x1], [by + dy, by + dy], color="#E0A100" if k == 0 else
                ("#E53935" if k == 3 else "#9E9E9E"), lw=3 if diagonal else 2, zorder=3)
    ax.text(bx - 1.55, by + 0.7, "pata 1\n(GPIO18)", ha="right", va="center", fontsize=8.3,
            color=TINTA)
    ax.text(bx + 1.55, by - 0.7, "pata 2\n(3V3)", ha="left", va="center", fontsize=8.3,
            color=TINTA)
    ax.text(bx, by + 1.35, "Pulsador", ha="center", va="bottom", fontsize=9, color=TINTA)
    ax.text(bx, by - 1.35, "use dos patas en diagonal", ha="center", va="top", fontsize=8.3,
            color=TINTA_2)

    ax.text(-2.7, -2.1, "Pines oscuros: mejor no usarlos (arranque, USB o UART). "
            "Guíese por la serigrafía de la placa.", fontsize=8.3, color=TINTA_2)

    fig.savefig(SALIDA / "fig3_conexiones.png", dpi=DPI, bbox_inches="tight",
                facecolor="white")
    plt.close(fig)
    print("   fig3_conexiones.png")


if __name__ == "__main__":
    print("Generando figuras en", SALIDA.relative_to(RAIZ))
    fig_circuito_clase()
    fig_circuito_interno()
    fig_conexiones()
