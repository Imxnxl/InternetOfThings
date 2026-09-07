"""
Genera las figuras del informe a partir de las trazas CSV de la simulacion.

Uso:  python scripts/generar_figuras.py
"""
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D

from figuras_comun import (SERIE, CRITICAL, GOOD, INK, INK_2, MUTED,
                           leer_csv, cargar_contexto, clave_fila,
                           banda_eventos, gantt_us, gantt_ticks,
                           leyenda, leyenda_al_pie, titulo, guardar)

RESUMEN = {}


def _ocupacion(filas):
    """Porcentaje de tiempo con el nucleo ocupado por algo que no sea IDLE."""
    total = filas[-1]["us1"] - filas[0]["us0"]
    ocupado = sum(f["dur"] for f in filas if not f["tarea"].startswith("IDLE"))
    return 100.0 * ocupado / total


# =========================================================== EJEMPLO 1 =====
def figura_ej1():
    filas = cargar_contexto(1)
    led = leer_csv("ej1_led.csv")

    mapa = {"ControlLed": SERIE[0], "LecturaSensor": SERIE[1], "Monitor": SERIE[2]}
    orden = ["ControlLed", "LecturaSensor", "Monitor", "infra", "IDLE"]

    fig = plt.figure(figsize=(11.4, 6.6))
    fig.subplots_adjust(top=0.83)
    gs = fig.add_gridspec(3, 2, height_ratios=[1.0, 1.7, 1.5],
                          width_ratios=[2.6, 1], hspace=0.85, wspace=0.20)
    ax1 = fig.add_subplot(gs[0, 0])
    ax2 = fig.add_subplot(gs[1, 0], sharex=ax1)
    ax3 = fig.add_subplot(gs[2, 0])
    ax4 = fig.add_subplot(gs[:, 1])

    # --- A: forma de onda del LED (tiempo simulado) ------------------------
    t = np.array([int(f["tick_ms"]) for f in led], dtype=float)
    v = np.array([int(f["estado"]) for f in led], dtype=float)
    ts, vs = np.append(t, 12000.0), np.append(v, v[-1])
    ax1.step(ts, vs, where="post", color=SERIE[0], linewidth=2.0)
    ax1.fill_between(ts, 0, vs, step="post", color=SERIE[0],
                     alpha=0.10, linewidth=0)
    ax1.set_yticks([0, 1])
    ax1.set_yticklabels(["apagado", "encendido"])
    ax1.set_ylim(-0.3, 1.6)
    ax1.set_ylabel("LED  GPIO 8", color=INK_2, fontsize=9)
    ax1.grid(axis="y", visible=False)
    ax1.annotate("", xy=(1001, 1.28), xytext=(2001, 1.28),
                 arrowprops=dict(arrowstyle="<->", color=MUTED, lw=1))
    ax1.text(2130, 1.28, "periodo 1000 ms  (500 encendido + 500 apagado)",
             fontsize=8.5, color=INK_2, va="center")
    ax1.set_title("A. Salida fisica: el LED del GPIO 8", fontsize=10,
                  color=INK, loc="left", pad=5)

    # --- B: cuando obtuvo el nucleo cada tarea -----------------------------
    banda_eventos(ax2, filas, orden, mapa, 0, 12000)
    ticks = np.arange(0, 12001, 1000)
    ax2.set_xticks(ticks)
    ax2.set_xticklabels([str(int(x / 1000)) for x in ticks])
    ax2.set_xlabel("tiempo simulado del ESP32-C6 (s)", color=INK_2, fontsize=9)
    ax2.set_title("B. Instantes en que cada tarea recibio el nucleo",
                  fontsize=10, color=INK, loc="left", pad=5)

    # --- C: detalle de un tick con dos tareas listas -----------------------
    us0 = next(f["us0"] for f in filas
               if f["tick"] == 1001 and f["tarea"] == "LecturaSensor")
    gantt_us(ax3, filas, orden, mapa, us0 - 40, us0 + 220)
    ax3.set_xlabel("microsegundos dentro del tick 1001 (reloj del simulador: "
                   "lo significativo es el orden, no la duracion)",
                   color=INK_2, fontsize=8.5)
    ax3.set_title("C. Detalle del instante t = 1001 ms, con las dos tareas listas a la vez",
                  fontsize=10, color=INK, loc="left", pad=5)
    ax3.text(0.63, 0.93, "el sensor entra primero por estar antes en\n"
                         "la lista de tareas listas de igual prioridad;\n"
                         "el LED va justo detras, y el nucleo vuelve\n"
                         "a quedar libre dentro del mismo tick",
             transform=ax3.transAxes, fontsize=8.5, color=INK_2, va="top")
    leyenda_al_pie(fig, orden, mapa, y=-0.02, ncol=5)

    # --- Cifras ------------------------------------------------------------
    pct = _ocupacion(filas)
    n_ctx = len(filas)
    RESUMEN["ej1_ocupacion"] = pct
    RESUMEN["ej1_cambios"] = n_ctx

    ax4.axis("off")
    ax4.text(0.0, 0.99, "Uso del nucleo", fontsize=10.5, color=INK,
             fontweight="bold", va="top")
    ax4.text(0.0, 0.90, "{:.3f} %".format(pct), fontsize=30, color=SERIE[0],
             va="top", fontweight="bold")
    ax4.text(0.0, 0.71, "Las tres tareas juntas ocupan\n"
                        "esa fraccion del tiempo. El resto\n"
                        "el nucleo esta en IDLE.",
             fontsize=9.5, color=INK_2, va="top")
    ax4.text(0.0, 0.50, "Cambios de contexto", fontsize=10.5, color=INK,
             fontweight="bold", va="top")
    ax4.text(0.0, 0.41, "{}".format(n_ctx), fontsize=30, color=SERIE[2],
             va="top", fontweight="bold")
    ax4.text(0.0, 0.22, "en 12 s. Cada uno es el kernel\n"
                        "guardando el estado de una tarea\n"
                        "y cargando el de la siguiente.",
             fontsize=9.5, color=INK_2, va="top")

    titulo(fig,
           "Figura 1 - Ejemplo 1: tres tareas concurrentes sobre un unico nucleo",
           "El LED conmuta cada 500 ms y el sensor muestrea cada 1000 ms, como pide el material. Las tres tareas\n"
           "parecen simultaneas, pero el nucleo solo atiende a una cada vez y pasa casi todo el tiempo libre.")
    guardar(fig, "fig1_ej1_concurrencia.png")


# =========================================================== EJEMPLO 2 =====
def figura_ej2():
    filas = cargar_contexto(2)
    eventos = leer_csv("ej2_eventos.csv")

    mapa = {"Productor": SERIE[0], "Consumidor": SERIE[1]}
    orden = ["Productor", "Consumidor", "infra", "IDLE"]

    fig = plt.figure(figsize=(11.4, 5.6))
    fig.subplots_adjust(top=0.80)
    gs = fig.add_gridspec(2, 2, width_ratios=[2.5, 1], hspace=1.05, wspace=0.20)
    ax1 = fig.add_subplot(gs[0, 0])
    ax2 = fig.add_subplot(gs[1, 0])
    ax3 = fig.add_subplot(gs[:, 1])

    banda_eventos(ax1, filas, orden, mapa, 0, 13000)
    ticks = np.arange(0, 13001, 2000)
    ax1.set_xticks(ticks)
    ax1.set_xticklabels([str(int(x / 1000)) for x in ticks])
    ax1.set_xlabel("tiempo simulado (s)", color=INK_2, fontsize=9)
    ax1.set_title("A. Actividad en 13 s: siete envios, nada mas",
                  fontsize=10, color=INK, loc="left", pad=5)

    us0 = next(f["us0"] for f in filas
               if f["tick"] == 2001 and f["tarea"] == "Productor")
    gantt_us(ax2, filas, orden, mapa, us0 - 60, us0 + 560)
    ax2.set_xlabel("microsegundos dentro del tick 2001 (reloj del simulador: "
                   "lo significativo es el orden, no la duracion)",
                   color=INK_2, fontsize=8.5)
    ax2.set_title("B. Detalle del envio numero 2: el productor encola y el consumidor "
                  "despierta acto seguido",
                  fontsize=10, color=INK, loc="left", pad=5)
    leyenda(ax2, orden, mapa, y=-0.72, ncol=4)

    pct = _ocupacion(filas)
    RESUMEN["ej2_ocupacion"] = pct
    RESUMEN["ej2_cambios"] = len(filas)

    ax3.axis("off")
    ax3.text(0.0, 0.99, "Latencia de la cola", fontsize=10.5, color=INK,
             fontweight="bold", va="top")
    ax3.text(0.0, 0.90, "0 ms", fontsize=38, color=GOOD, va="top",
             fontweight="bold")
    ax3.text(0.0, 0.68, "en las {} muestras recibidas.\n"
                        "El consumidor despierta dentro\n"
                        "del mismo tick del envio.".format(len(eventos)),
             fontsize=9.5, color=INK_2, va="top")
    ax3.text(0.0, 0.46, "Uso del nucleo", fontsize=10.5, color=INK,
             fontweight="bold", va="top")
    ax3.text(0.0, 0.37, "{:.4f} %".format(pct), fontsize=27, color=SERIE[0],
             va="top", fontweight="bold")
    ax3.text(0.0, 0.18, "Solo {} cambios de contexto en\n"
                        "13 s. Esperar bloqueado en la cola\n"
                        "no consume tiempo de CPU: es la\n"
                        "diferencia con hacer polling.".format(len(filas)),
             fontsize=9.5, color=INK_2, va="top")

    titulo(fig,
           "Figura 2 - Ejemplo 2: cola productor / consumidor",
           "El consumidor espera en xQueueReceive(..., portMAX_DELAY). No sondea la cola: duerme hasta que llega\n"
           "el dato y lo recibe sin retraso medible.")
    guardar(fig, "fig2_ej2_cola.png")


# =========================================================== EJEMPLO 3 =====
def figura_ej3():
    filas = cargar_contexto(3)
    eventos = leer_csv("ej3_eventos.csv")

    def evento(nombre):
        return next((int(e["valor"]) for e in eventos
                     if e["etiqueta"] == nombre), None)

    def tick_evento(nombre):
        return next((int(e["tick_ms"]) for e in eventos
                     if e["etiqueta"] == nombre), None)

    twdt = tick_evento("twdt_timeout_ms")
    hueco = evento("hueco_victima_ms")
    perdidos = evento("periodos_perdidos")
    RESUMEN.update({"ej3_twdt": twdt, "ej3_hueco": hueco,
                    "ej3_perdidos": perdidos})

    mapa = {"Golosa": SERIE[1], "Victima": SERIE[0], "Fondo": SERIE[2]}
    orden = ["Golosa", "Victima", "Fondo", "infra", "IDLE"]

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(11.4, 6.4),
                                   height_ratios=[1.55, 1.0])
    fig.subplots_adjust(top=0.82, hspace=0.95)

    banda_eventos(ax1, filas, orden, mapa, 0, 20000)
    ax1.set_ylim(5.6, -0.55)          # hueco inferior para rotular las fases
    ax1.axvspan(5001, 13001, color=CRITICAL, alpha=0.07, linewidth=0, zorder=0)
    ax1.axvline(5001, color=CRITICAL, linewidth=1.1, alpha=0.5)
    ax1.axvline(13001, color=GOOD, linewidth=1.1, alpha=0.5)
    ax1.text(9000, 5.25, "FASE B - la tarea de prioridad 3 no cede el nucleo",
             ha="center", fontsize=9, color=CRITICAL, fontweight="bold")
    ax1.text(2450, 5.25, "FASE A - reparto correcto", ha="center",
             fontsize=9, color=INK_2)
    ax1.text(16600, 5.25, "FASE C - recuperacion", ha="center",
             fontsize=9, color=INK_2)

    ax1.axvline(twdt, color=CRITICAL, linewidth=1.6, linestyle=(0, (4, 2)))
    ax1.annotate("el watchdog dispara a los {} ms:\n"
                 "un ESP32-C6 real se reiniciaria aqui".format(twdt),
                 xy=(twdt, 3.55), xytext=(5350, 4.35),
                 fontsize=9, color=CRITICAL, fontweight="bold",
                 va="center", ha="left",
                 arrowprops=dict(arrowstyle="->", color=CRITICAL, lw=1.1))

    ticks = np.arange(0, 20001, 2000)
    ax1.set_xticks(ticks)
    ax1.set_xticklabels([str(int(x / 1000)) for x in ticks])
    ax1.set_xlabel("tiempo simulado del ESP32-C6 (s)", color=INK_2, fontsize=9)
    ax1.set_title("A. Instantes en que cada tarea recibio el nucleo",
                  fontsize=10, color=INK, loc="left", pad=5)
    leyenda_al_pie(fig, orden, mapa, y=-0.03, ncol=6, extra=[
        Line2D([0], [0], color=CRITICAL, lw=1.6, linestyle=(0, (4, 2)),
               label="disparo del watchdog (TWDT)")])

    # --- B: separacion real entre latidos de la tarea victima --------------
    t_v, huecos = [], []
    ultimo = None
    for f in filas:
        if f["tarea"] == "Victima":
            if ultimo is not None and f["tick"] - ultimo > 1:
                t_v.append(f["tick"] / 1000.0)
                huecos.append(f["tick"] - ultimo)
            ultimo = f["tick"]

    ax2.step(t_v, huecos, where="post", color=SERIE[0], linewidth=1.8)
    ax2.axhline(100, color=GOOD, linewidth=1.2)
    ax2.text(0.25, 116, "periodo nominal: 100 ms", fontsize=8.5, color=GOOD)
    ax2.set_yscale("log")
    ax2.set_ylabel("separacion entre\nlatidos (ms, log)", color=INK_2, fontsize=9)
    ax2.set_xlabel("tiempo simulado del ESP32-C6 (s)", color=INK_2, fontsize=9)
    ax2.set_xlim(0, 20)
    ax2.set_xticks(np.arange(0, 21, 2))
    ax2.set_title("B. Puntualidad de la tarea victima (prioridad 2, periodo 100 ms)",
                  fontsize=10, color=INK, loc="left", pad=5)
    if huecos:
        pico = max(huecos)
        ip = huecos.index(pico)
        ax2.annotate("{} ms sin ejecutarse = {} periodos perdidos".format(
                         hueco, perdidos),
                     xy=(t_v[ip], pico), xytext=(t_v[ip] - 9.6, pico * 0.42),
                     fontsize=9, color=CRITICAL, fontweight="bold",
                     arrowprops=dict(arrowstyle="->", color=CRITICAL, lw=1.1))

    titulo(fig,
           "Figura 3 - Ejemplo 3: inanicion (starvation) y disparo del watchdog",
           "Una tarea de prioridad 3 entra en un bucle de calculo de 8 s sin llamar a vTaskDelay(). Ninguna otra tarea,\n"
           "ni siquiera la tarea Idle, vuelve a ejecutarse hasta que la suelta.")
    guardar(fig, "fig3_ej3_inanicion.png")


# =========================================================== EJEMPLO 4 =====
def figura_ej4():
    filas = cargar_contexto(4)

    mapa = {"Calculo": SERIE[2], "Urgente": SERIE[1]}
    orden = ["Urgente", "Calculo", "infra", "IDLE"]

    fig = plt.figure(figsize=(11.4, 4.6))
    fig.subplots_adjust(top=0.76)
    gs = fig.add_gridspec(1, 2, width_ratios=[2.4, 1], wspace=0.22)
    ax1 = fig.add_subplot(gs[0, 0])
    ax2 = fig.add_subplot(gs[0, 1])

    urgentes = [f for f in filas if f["tarea"] == "Urgente" and f["tick"] == 2001]
    us_u = urgentes[0]["us0"]
    gantt_us(ax1, filas, orden, mapa, us_u - 700, us_u + 700)
    ax1.axvline(700, color=CRITICAL, linewidth=1.4, linestyle=(0, (4, 2)))
    ax1.annotate("vence el vTaskDelayUntil de Urgente (tick 2001):\n"
                 "el planificador corta la rodaja de Calculo a media\n"
                 "ejecucion y le entrega el nucleo en el acto",
                 xy=(700, 1.42), xytext=(780, 2.45),
                 fontsize=9, color=CRITICAL, va="center",
                 arrowprops=dict(arrowstyle="->", color=CRITICAL, lw=1.1))
    ax1.text(15, 2.45, "cada bloque es una rodaja distinta:\n"
                       "Calculo cede el turno con taskYIELD()",
             fontsize=8.5, color=INK_2, va="center")
    ax1.set_xlabel("microsegundos alrededor del evento critico (reloj del simulador: "
                   "lo significativo es el orden)", color=INK_2, fontsize=8.5)
    leyenda(ax1, orden, mapa, y=-0.30, ncol=4)

    ax2.axis("off")
    ax2.text(0.0, 0.99, "Latencia de preemcion", fontsize=10.5, color=INK,
             fontweight="bold", va="top")
    ax2.text(0.0, 0.89, "0 ticks", fontsize=34, color=GOOD, va="top",
             fontweight="bold")
    ax2.text(0.0, 0.68, "en las 5 activaciones medidas,\n"
                        "con el nucleo al 100 % ocupado\n"
                        "por la tarea de prioridad 1.",
             fontsize=9.5, color=INK_2, va="top")
    ax2.text(0.0, 0.44, "Lectura", fontsize=10.5, color=INK,
             fontweight="bold", va="top")
    ax2.text(0.0, 0.34, "La prioridad manda sobre la carga:\n"
                        "una tarea que satura el nucleo no\n"
                        "retrasa a otra de prioridad superior.\n"
                        "Es lo que permite que el Wi-Fi del\n"
                        "ESP32-C6 (prioridades 18-23) siga\n"
                        "funcionando sobre codigo de usuario.",
             fontsize=9.5, color=INK_2, va="top")

    titulo(fig,
           "Figura 4 - Ejemplo 4: preemcion inmediata por prioridad",
           "Calculo (prioridad 1) ocupa el nucleo sin descanso. Urgente (prioridad 5) despierta cada 2 s y obtiene\n"
           "la CPU en el mismo tick, sin esperar a que la otra termine su trabajo.")
    guardar(fig, "fig4_ej4_preemcion.png")


# =========================================================== EJEMPLO 5 =====
def figura_ej5():
    filas = cargar_contexto(5)

    mapa = {"TrabajoA": SERIE[0], "TrabajoB": SERIE[1]}
    orden = ["TrabajoA", "TrabajoB", "infra", "IDLE"]

    fig = plt.figure(figsize=(11.4, 4.8))
    fig.subplots_adjust(top=0.76)
    gs = fig.add_gridspec(1, 2, width_ratios=[2.4, 1], wspace=0.26)
    ax1 = fig.add_subplot(gs[0, 0])
    ax2 = fig.add_subplot(gs[0, 1])

    gantt_ticks(ax1, filas, orden, mapa, 1000, 1019)
    ax1.set_xticks(np.arange(1000, 1021, 2))
    ax1.set_xlabel("tiempo simulado (ms) - 20 ticks consecutivos", color=INK_2)
    ax1.set_title("Un cambio de contexto en cada tick de 1 ms",
                  fontsize=10, color=INK, loc="left", pad=6)
    ax1.text(1000.3, 3.05, "la fila IDLE esta vacia: con dos tareas siempre listas,\n"
                           "la tarea Idle no llega a ejecutarse ni una sola vez",
             fontsize=8.5, color=INK_2, va="center")
    leyenda(ax1, orden, mapa, y=-0.28, ncol=4)

    reparto = {}
    for f in filas:
        if f["tarea"] in ("TrabajoA", "TrabajoB"):
            reparto[f["tarea"]] = reparto.get(f["tarea"], 0.0) + f["dur"]
    total = sum(reparto.values())
    etiquetas = ["TrabajoA", "TrabajoB"]
    valores = [100.0 * reparto[e] / total for e in etiquetas]
    RESUMEN["ej5_reparto"] = valores
    RESUMEN["ej5_cambios"] = len(filas)

    barras = ax2.barh(etiquetas, valores,
                      color=[mapa[e] for e in etiquetas], height=0.22)
    for b, v in zip(barras, valores):
        ax2.text(v - 2.5, b.get_y() + b.get_height() / 2, "{:.1f} %".format(v),
                 va="center", ha="right", color="white", fontsize=11,
                 fontweight="bold")
    ax2.set_xlim(0, 100)
    ax2.set_xlabel("cuota acumulada del nucleo (%)", color=INK_2, fontsize=9)
    ax2.grid(axis="y", visible=False)
    ax2.invert_yaxis()
    ax2.set_title("Reparto tras 6 s de competencia", fontsize=10, color=INK,
                  loc="left", pad=6)

    titulo(fig,
           "Figura 5 - Ejemplo 5: reparto por turnos (time-slicing) entre tareas iguales",
           "Dos tareas de prioridad 2 que no se bloquean nunca. FreeRTOS da un tick a cada una, alternandolas,\n"
           "hasta repartir el nucleo exactamente al 50 %.")
    guardar(fig, "fig5_ej5_turnos.png")


if __name__ == "__main__":
    figura_ej1()
    figura_ej2()
    figura_ej3()
    figura_ej4()
    figura_ej5()
    print()
    for k, v in RESUMEN.items():
        print("  {:18s} {}".format(k, v))
