"""
graficas_placa.py - Graficas con los datos reales medidos en la placa.

Lee los registros del monitor serie guardados en docs/evidencias/ y genera:

    fig3_barrido.png       Lectura cruda del ADC al girar la perilla (primer programa)
    fig4_calibracion.png   Tension calibrada frente a lectura cruda, con el ajuste lineal

Ademas imprime las cifras que cita el informe (ajuste, tope de la perilla...).

Uso:   python scripts/graficas_placa.py
"""
import re
import statistics as st
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

RAIZ = Path(__file__).resolve().parent.parent
EVID = RAIZ / "docs" / "evidencias"

SUPERFICIE = "#FFFFFF"
TINTA = "#0B0B0B"
TINTA_2 = "#52514E"
REJILLA = "#E4E3DE"
SERIE_1 = "#2A78D6"
DPI = 200
PERIODO_S = 0.1          # el firmware informa cada 100 ms


def leer(nombre):
    texto = (EVID / nombre).read_text(encoding="utf-8", errors="replace")
    return [(int(a), int(m)) for a, m in re.findall(r"adc:(\d+) mV:(\d+)", texto)]


def estilo(ax):
    for lado in ("top", "right"):
        ax.spines[lado].set_visible(False)
    for lado in ("left", "bottom"):
        ax.spines[lado].set_color(REJILLA)
    ax.tick_params(colors=TINTA_2, labelsize=9, length=0)
    ax.grid(color=REJILLA, lw=0.8)
    ax.set_axisbelow(True)


def ajuste_lineal(xs, ys):
    n = len(xs)
    mx, my = sum(xs) / n, sum(ys) / n
    sxx = sum((x - mx) ** 2 for x in xs)
    sxy = sum((x - mx) * (y - my) for x, y in zip(xs, ys))
    a = sxy / sxx
    b = my - a * mx
    res = [y - (a * x + b) for x, y in zip(xs, ys)]
    r2 = 1 - sum(r * r for r in res) / sum((y - my) ** 2 for y in ys)
    return a, b, r2, st.pstdev(res)


def main():
    datos = leer("monitor_pruebas.txt")
    # Hasta los 119 s son los barridos; despues, la perilla quieta para la prueba de ruido.
    barrido = datos[:1190]
    t = [i * PERIODO_S for i in range(len(barrido))]
    adc = [a for a, _ in barrido]
    tope = [a for a, m in datos if a >= 3425]
    tope_medio = sum(tope) / len(tope)

    # ---------------------------------------------------------------- fig 3
    fig, ax = plt.subplots(figsize=(8.6, 3.9))
    fig.patch.set_facecolor(SUPERFICIE)
    ax.plot(t, adc, color=SERIE_1, lw=2)
    for y, texto in [(4095, "4095: máximo del ADC de 12 bits (nunca se alcanza)"),
                     (tope_medio, f"{tope_medio:.0f}: tope real de la perilla, 3,31 V"
                                  f" (= {tope_medio / 4095 * 100:.0f} % de 4095)")]:
        ax.axhline(y, color=TINTA_2, lw=1.1, ls=(0, (4, 3)))
        ax.text(1, y + 60, texto, fontsize=8.8, color=TINTA_2, va="bottom")
    ax.set_xlim(0, t[-1])
    ax.set_ylim(0, 4450)
    ax.set_yticks([0, 1024, 2048, 3072, 4095])
    ax.set_xlabel("tiempo (s)", fontsize=9, color=TINTA_2)
    ax.set_title("Lectura cruda del ADC (analogRead) al girar la perilla de tope a tope",
                 loc="left", fontsize=10.5, color=TINTA, pad=8)
    estilo(ax)
    fig.tight_layout()
    fig.savefig(EVID / "fig3_barrido.png", dpi=DPI, facecolor=SUPERFICIE)
    plt.close(fig)

    # ---------------------------------------------------------------- fig 4
    xs = [a for a, m in datos if a > 50]
    ys = [m for a, m in datos if a > 50]
    a, b, r2, desv = ajuste_lineal(xs, ys)

    fig, ax = plt.subplots(figsize=(6.8, 5.0))
    fig.patch.set_facecolor(SUPERFICIE)
    ax.scatter([x for x, _ in datos], [m for _, m in datos], s=9, color=SERIE_1,
               edgecolors="none", alpha=0.55, label="medido en la placa", zorder=3)
    ax.plot([0, 4095], [0, 3300], color=TINTA_2, lw=1.4, ls=(0, (4, 3)),
            label="supuesto habitual: 4095 = 3300 mV", zorder=2)
    ax.annotate(f"tope de la perilla\n{tope_medio:.0f} cuentas → 3310 mV",
                xy=(tope_medio, 3310), xytext=(1500, 3560), fontsize=8.8, color=TINTA,
                arrowprops=dict(arrowstyle="-", color=TINTA_2, lw=0.9))
    ax.text(2500, 1550, (f"medido:  mV = {a:.4f} · lectura {b:+.1f}\nR² = {r2:.5f}")
            .replace(".", ","), fontsize=8.8, color=TINTA)
    ax.text(3480, 2380, "supuesto\n4095 = 3300 mV", fontsize=8.8, color=TINTA_2)
    ax.set_xlim(0, 4200)
    ax.set_ylim(0, 4000)
    ax.set_xticks([0, 1024, 2048, 3072, 4095])
    ax.set_xlabel("lectura cruda (analogRead)", fontsize=9, color=TINTA_2)
    ax.set_ylabel("tensión calibrada (analogReadMilliVolts), mV", fontsize=9, color=TINTA_2)
    ax.set_title("Tensión real frente a lectura cruda", loc="left", fontsize=10.5,
                 color=TINTA, pad=8)
    leyenda = ax.legend(loc="lower right", fontsize=8.8, frameon=False)
    for texto in leyenda.get_texts():
        texto.set_color(TINTA)
    estilo(ax)
    fig.tight_layout()
    fig.savefig(EVID / "fig4_calibracion.png", dpi=DPI, facecolor=SUPERFICIE)
    plt.close(fig)

    print(f"barrido: {len(barrido)} muestras ({t[-1]:.0f} s), lectura {min(adc)}..{max(adc)}")
    print(f"tope: {len(tope)} muestras, media {tope_medio:.1f} ({tope_medio / 4095 * 100:.1f} % de 4095)")
    print(f"ajuste: mV = {a:.4f} * lectura {b:+.1f} | R2 = {r2:.5f} | desv. residuos {desv:.1f} mV"
          f" | 4095 -> {a * 4095 + b:.0f} mV")
    print("   fig3_barrido.png, fig4_calibracion.png")


if __name__ == "__main__":
    main()
