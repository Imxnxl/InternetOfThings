"""
generar_figuras.py - Figuras explicativas del informe (no contienen datos medidos).

    fig1_circuito.png      Esquema: potenciometro -> ADC -> PWM -> LED
    fig2_pwm.png           Como una lectura del ADC se convierte en ciclo de trabajo

Las graficas con datos reales de la placa las genera graficas_placa.py.

Uso:   python scripts/generar_figuras.py
Requiere:  pip install schemdraw matplotlib
"""
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

import schemdraw
import schemdraw.elements as elm
import schemdraw.flow as flow

schemdraw.use("matplotlib")

RAIZ = Path(__file__).resolve().parent.parent
SALIDA = RAIZ / "docs" / "evidencias"
SALIDA.mkdir(parents=True, exist_ok=True)

# Paleta de referencia, modo claro (texto con tinta, nunca con el color de la serie)
SUPERFICIE = "#FFFFFF"
TINTA = "#0B0B0B"
TINTA_2 = "#52514E"
REJILLA = "#E4E3DE"
SERIE_1 = "#2A78D6"      # azul: la senal PWM
ACENTO = "#1C5CAB"       # bloques internos del chip
VERDE_LED = "#008300"    # el LED es verde de verdad: identidad del componente
DPI = 200


# ------------------------------------------------------------------ fig 1 --
def fig_circuito():
    with schemdraw.Drawing(show=False) as d:
        d.config(fontsize=11, unit=2.4, lw=1.4, color=TINTA)

        # Potenciometro como divisor de tension entre 3V3 y GND
        elm.Vdd().label("3V3")
        pot = elm.Potentiometer().down().label("Potenciómetro", loc="top", ofst=0.35)
        elm.Ground()

        # Cursor -> GPIO2
        elm.Line().at(pot.tap).right(d.unit * 0.55)
        pin_adc = elm.Dot(open=True).label("GPIO2", loc="top", ofst=0.12)
        elm.Label().at((pot.tap[0] + 0.1, pot.tap[1] - 0.55)).label(
            "0 … 3,3 V", fontsize=10, color=TINTA_2, halign="left")

        # Bloques internos del chip
        elm.Line().right(d.unit * 0.45).at(pin_adc.center)
        adc = flow.Box(w=2.6, h=1.3).anchor("W").color(ACENTO).label(
            "ADC\n12 bits", color=ACENTO)
        a1 = elm.Arrow().right(d.unit * 0.9).at(adc.E).color(TINTA_2).label(
            "0 … 4095", loc="top", fontsize=10, color=TINTA_2)
        pwm = flow.Box(w=2.9, h=1.3).anchor("W").color(ACENTO).label(
            "PWM (LEDC)\n12 bits · 5 kHz", color=ACENTO)
        elm.Label().at((a1.center[0], a1.center[1] - 1.05)).label(
            "analogRead() → ledcWrite()", fontsize=9.5, color=TINTA_2)
        elm.Line().right(d.unit * 0.45).at(pwm.E)
        pin_pwm = elm.Dot(open=True).label("GPIO20", loc="top", ofst=0.12)

        elm.EncircleBox([adc, a1, pwm], padx=0.55, pady=0.95).linestyle("--").color(
            TINTA_2).label("dentro del ESP32-C6", loc="bottom", fontsize=10, color=TINTA_2)

        # LED verde -> GND
        elm.Line().right(d.unit * 0.5).at(pin_pwm.center)
        elm.LED().down().color(VERDE_LED).label("LED G", loc="bottom", ofst=0.3,
                                                color=TINTA)
        elm.Ground()

        d.save(str(SALIDA / "fig1_circuito.png"), dpi=DPI)
    print("   fig1_circuito.png")


# ------------------------------------------------------------------ fig 2 --
def fig_pwm():
    """Tres lecturas del ADC y la senal PWM que producen (5 kHz: periodo de 200 us)."""
    casos = [(1024, "0,83 V"), (2048, "1,65 V"), (3072, "2,48 V")]
    periodo_us = 200.0
    t = np.linspace(0, 3 * periodo_us - 0.01, 3000)

    fig, ejes = plt.subplots(len(casos), 1, figsize=(8.6, 5.4), sharex=True)
    fig.patch.set_facecolor(SUPERFICIE)
    for ax, (lectura, tension) in zip(ejes, casos):
        ciclo = lectura / 4095
        v = np.where((t % periodo_us) < ciclo * periodo_us, 3.3, 0.0)
        ax.plot(t, v, color=SERIE_1, lw=2, solid_joinstyle="miter")
        ax.plot([0, 3 * periodo_us], [3.3 * ciclo] * 2, color=TINTA_2, lw=1.2, ls=(0, (4, 3)))
        ax.text(3 * periodo_us + 8, 3.3 * ciclo, f"media {3.3 * ciclo:.2f} V".replace(".", ","),
                va="center", fontsize=9, color=TINTA_2)
        ax.set_title(f"cursor a {tension}  →  analogRead() = {lectura}  →  "
                     f"ciclo de trabajo {ciclo * 100:.0f} %",
                     loc="left", fontsize=10, color=TINTA, pad=4)
        ax.set_ylim(-0.3, 3.8)
        ax.set_yticks([0, 3.3])
        ax.set_yticklabels(["0 V", "3,3 V"], fontsize=9, color=TINTA_2)
        ax.set_xlim(0, 3 * periodo_us + 90)
        for lado in ("top", "right"):
            ax.spines[lado].set_visible(False)
        for lado in ("left", "bottom"):
            ax.spines[lado].set_color(REJILLA)
        ax.tick_params(colors=TINTA_2, length=0)
        ax.grid(axis="x", color=REJILLA, lw=0.8)
        ax.set_axisbelow(True)
    ejes[-1].set_xticks([0, 200, 400, 600])
    ejes[-1].set_xticklabels(["0", "200 µs", "400 µs", "600 µs"], fontsize=9)
    fig.text(0.01, 0.005, "Señal en GPIO20 (PWM a 5 kHz). La línea discontinua es la "
             "tensión media, que es lo que determina el brillo del LED.",
             fontsize=8.5, color=TINTA_2)
    fig.tight_layout(rect=(0, 0.03, 1, 1))
    fig.savefig(SALIDA / "fig2_pwm.png", dpi=DPI, facecolor=SUPERFICIE)
    plt.close(fig)
    print("   fig2_pwm.png")


if __name__ == "__main__":
    print("Generando figuras en", SALIDA.relative_to(RAIZ))
    fig_circuito()
    fig_pwm()
