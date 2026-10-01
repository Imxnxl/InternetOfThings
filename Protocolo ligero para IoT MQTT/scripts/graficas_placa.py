"""
graficas_placa.py - Graficas y cifras con los datos reales medidos en la placa.

Lee los registros guardados en docs/evidencias/ y genera:

    fig2_arranque.png      Del reinicio a la primera publicacion (reloj del chip)
    fig3_temperatura.png   Temperatura publicada: primera version frente a version final
    fig4_latencia.png      Tiempo que tarda un mensaje en cruzar el broker
    fig5_bytes.png         Bytes por lectura: MQTT frente a HTTP

Ademas imprime las cifras que cita el informe.

Registros (docs/evidencias/):
    monitor_publicador_v1.txt / pc_publicador_v1.txt     primera version, una lectura por muestra (10 min)
    monitor_publicador.txt / pc_publicador.txt           version final, mediana de 5 lecturas (15 min)
    monitor_publicador_latencia.txt / pc_publicador_latencia.txt
                                                         version final con la captura corregida (7 min)
    monitor_suscriptor.txt / pc_suscriptor.txt           26 comandos, con casos limite
    monitor_suscriptor_sin_ahorro.txt / pc_suscriptor_sin_ahorro.txt
                                                         20 comandos con WIFI_SIEMPRE_ENCENDIDO = 1
    monitor_suscriptor_latencia.txt / pc_suscriptor_latencia.txt / pc_observador_latencia.txt
                                                         20 comandos con la captura corregida, y los
                                                         mismos recibidos por otro cliente del PC
    peticion_http.txt                                    la misma lectura enviada por HTTP

Los registros anteriores a la correccion de scripts/capturar_monitor.py sellan
las lineas de la placa con un retraso fijo (seccion 5.4 del informe): sirven
para todo menos para medir latencias absolutas.

Uso:   python scripts/graficas_placa.py
Requiere:  pip install matplotlib
"""
import json
import re
import statistics as st
from datetime import timedelta

import matplotlib.pyplot as plt
from matplotlib.ticker import MultipleLocator

from figuras_comun import EJE, EVID, SERIE_1, TINTA, TINTA_2, estilo, guardar, leer_pc, leer_serie

BAUDIOS = 115200
COLOR_ANOMALIA = "#d03b3b"     # rojo de estado "critico": una lectura erronea, con su etiqueta


def t_uart(texto):
    """Tiempo que tarda una linea en salir por la UART (8N1: 10 bits por caracter, mas CR LF)."""
    return timedelta(seconds=(len(texto) + 2) * 10 / BAUDIOS)


def coma(x, dec=1):
    return f"{x:.{dec}f}".replace(".", ",")


def resumen(nombre, datos, unidad="ms", dec=0):
    f = f"{{:.{dec}f}}"
    return (f"{nombre}: n={len(datos)}  mediana={f.format(st.median(datos))} {unidad}  "
            f"media={f.format(st.mean(datos))}  min={f.format(min(datos))}  max={f.format(max(datos))}")


# ------------------------------------------------------------- publicador --

def datos_publicador(serie_txt, pc_txt):
    eventos, fallos, publicaciones, anomalias = {}, [], [], []
    for hora, ms, tag, msg in leer_serie(serie_txt):
        if ms is None:
            continue
        linea = f"I ({ms}) {tag}: {msg}"
        if "Calling app_main" in msg:
            eventos.setdefault("app_main", ms)
        elif msg.startswith("Sensor de temperatura listo"):
            eventos["sensor"] = ms
            eventos["temp_wifi_apagado"] = float(re.search(r"([\d.]+) C", msg)[1])
        elif msg.startswith("Connecting to"):
            eventos.setdefault("wifi_inicio", ms)
        elif "Wi-Fi disconnected" in msg:
            fallos.append((ms, int(re.search(r"disconnected (\d+)", msg)[1])))
        elif tag == "wifi" and msg.startswith("connected with"):
            eventos.setdefault("wifi_asociado", ms)
        elif msg.startswith("Got IPv4 event"):
            eventos.setdefault("ip", ms)
        elif msg.startswith("Conectado exitosamente al Broker"):
            eventos.setdefault("mqtt", ms)
        elif msg.startswith("Lectura anomala descartada"):
            anomalias.append((ms, float(re.search(r"descartada: ([\d.]+) C", msg)[1])))
        elif msg.startswith("Temperatura "):
            valor = float(re.search(r"field1=([\d.]+)", msg)[1])
            publicaciones.append((hora - t_uart(linea), ms, valor))

    recibidas = [(h, float(re.search(r"field1=([\d.]+)", t)[1]))
                 for h, t in leer_pc(pc_txt) if "/publish" in t and "field1=" in t]
    return eventos, fallos, publicaciones, recibidas, anomalias


def latencias_publicador(publicaciones, recibidas):
    """Cada publicacion con la primera recepcion del PC posterior que trae el
    mismo valor (ventana de 15 s, menor que el periodo de 20 s)."""
    lat, usadas = [], set()
    for envio, ms, valor in publicaciones:
        for i, (hora_pc, v) in enumerate(recibidas):
            dt = (hora_pc - envio).total_seconds()
            if i not in usadas and v == valor and -0.05 < dt < 15:
                lat.append(dt * 1000)
                usadas.add(i)
                break
    return lat


def informe_publicador(etiqueta, datos):
    eventos, fallos, pubs, recibidas, anomalias = datos
    periodos_chip = [b[1] - a[1] for a, b in zip(pubs, pubs[1:])]
    valores = [v for _, _, v in pubs]
    print(f"== Publicador ({etiqueta})")
    print("   eventos (ms desde el reinicio):", eventos)
    print("   intentos de Wi-Fi rechazados:", fallos)
    print(f"   publicaciones en la placa: {len(pubs)}  recibidas en el PC: {len(recibidas)}")
    print("  ", resumen("periodo segun el chip", periodos_chip))
    print(f"   temperaturas: min={min(valores)} max={max(valores)}  valores distintos={sorted(set(valores))}")
    print(f"   lecturas anomalas descartadas: {len(anomalias)} {anomalias}")
    return latencias_publicador(pubs, recibidas)


# -------------------------------------------------------------- suscriptor --

def recepciones_placa(serie_txt):
    """(hora en que la placa empieza a avisar, contenido, accion) por mensaje."""
    serie = leer_serie(serie_txt)
    recepciones = []
    for i, (hora, ms, tag, msg) in enumerate(serie):
        if ms is not None and msg.startswith("NOTIFICACION RECIBIDA"):
            aviso = hora - t_uart(f"I ({ms}) {tag}: {msg}")
            datos = next(m[len("Datos recibidos: "):] for h, s, t, m in serie[i + 1:i + 4]
                         if s is None and m.startswith("Datos recibidos: "))
            accion = next(m for h, s, t, m in serie[i + 1:i + 5] if s is not None)
            recepciones.append((aviso, datos, accion))
    return recepciones


def envios_pc(pc_txt):
    return [(h, m[1]) for h, t in leer_pc(pc_txt)
            if (m := re.match(r"enviado field1=(.*) a channels/\S+/subscribe$", t))]


def entry_id(texto):
    try:
        return json.loads(texto).get("entry_id")
    except (ValueError, AttributeError):
        return None


def por_entrada(envios, llegadas):
    """Latencias emparejando por entry_id: el k-esimo envio lleva entry_id = k."""
    hora_envio = {k + 1: h for k, (h, _) in enumerate(envios)}
    return [(h - hora_envio[e]).total_seconds() * 1000 for h, datos in llegadas
            if (e := entry_id(datos)) in hora_envio]


def emparejar_por_contenido(recepciones, envios):
    """Para el registro con casos limite (algunos sin JSON): cada envio con la
    recepcion que trae su contenido. Devuelve (valor, latencia o None, accion)."""
    resultado, usadas = [], set()
    for hora_envio, valor in envios:
        encontrado = None
        for i, (hora_rx, datos, accion) in enumerate(recepciones):
            if i in usadas:
                continue
            if valor.startswith("raw:"):
                coincide = datos == valor[4:]
            elif valor == "null":
                coincide = '"field1":null' in datos
            else:
                coincide = f'"field1":"{valor}"' in datos
            dt = (hora_rx - hora_envio).total_seconds()
            if coincide and -0.05 < dt < 6:
                encontrado = (valor, dt * 1000, accion)
                usadas.add(i)
                break
        resultado.append(encontrado or (valor, None, "no llegó"))
    return resultado


# ---------------------------------------------------------------- figuras --

def fig_arranque(eventos, fallos):
    fases = [
        ("Arranque del chip y del sensor", 0, eventos["sensor"]),
        ("Asociación al Wi-Fi", eventos["wifi_inicio"], eventos["wifi_asociado"]),
        ("Dirección IP (DHCP)", eventos["wifi_asociado"], eventos["ip"]),
        ("Conexión con el broker", eventos["ip"], eventos["mqtt"]),
    ]
    fig, ax = plt.subplots(figsize=(9.2, 3.2))
    estilo(ax)
    ax.grid(axis="y", visible=False)
    for i, (nombre, ini, fin) in enumerate(fases):
        y = len(fases) - 1 - i
        ax.barh(y, (fin - ini) / 1000, left=ini / 1000, height=0.42, color=SERIE_1)
        ax.text(fin / 1000 + 0.15, y, f"{coma((fin - ini) / 1000, 2)} s", va="center", fontsize=9, color=TINTA_2)
    y_wifi = len(fases) - 2
    for ms, motivo in fallos:
        ax.plot(ms / 1000, y_wifi, marker="x", ms=7, mew=1.6, color=TINTA, zorder=4)
    if fallos:
        ax.text(fallos[0][0] / 1000, y_wifi + 0.32, f"{len(fallos)} intentos rechazados (motivo {fallos[0][1]})",
                fontsize=8.6, color=TINTA_2, va="bottom")
    ax.axvline(eventos["mqtt"] / 1000, color=EJE, lw=1)
    ax.text(eventos["mqtt"] / 1000 - 0.15, len(fases) - 0.45, f"primera publicación\n{coma(eventos['mqtt'] / 1000, 2)} s",
            ha="right", va="top", fontsize=8.6, color=TINTA)
    ax.set_yticks(range(len(fases)), [f[0] for f in reversed(fases)])
    ax.set_xlabel("Tiempo desde el reinicio (s, reloj del chip)")
    ax.xaxis.set_major_locator(MultipleLocator(2))      # enteros: sin separador decimal en el eje
    ax.set_xlim(0, eventos["mqtt"] / 1000 + 2.2)
    ax.set_ylim(-0.6, len(fases) - 0.2)
    guardar(fig, "fig2_arranque.png")


def fig_temperatura(v1, final):
    """Dos paneles con el mismo eje Y: la anomalia de la primera version se ve a
    escala frente a la variacion normal."""
    todos = [v for _, _, v in v1[2]] + [v for _, _, v in final[2]]
    lo, hi = min(todos) - 2, max(todos) + 3
    fig, ejes = plt.subplots(1, 2, figsize=(10.4, 3.7), sharey=True,
                             gridspec_kw={"width_ratios": [2, 3], "wspace": 0.06})
    paneles = [(ejes[0], v1, "Primera versión: una lectura por muestra"),
               (ejes[1], final, "Versión final: mediana de 5 lecturas")]
    for ax, (eventos, _, pubs, _, _), titulo in paneles:
        estilo(ax)
        t = [ms / 60000 for _, ms, _ in pubs]
        v = [valor for _, _, valor in pubs]
        normal = [(x, y) for x, y in zip(t, v) if y < 45]
        ax.plot(t, v, color=SERIE_1, lw=2, solid_joinstyle="round")
        ax.plot([x for x, _ in normal], [y for _, y in normal], "o", ms=3.6, color=SERIE_1, mec="white", mew=0.7)
        for x, y in zip(t, v):
            if y >= 45:
                ax.plot(x, y, "o", ms=7, color=COLOR_ANOMALIA, mec="white", mew=1.2, zorder=5)
                ax.annotate(f"lectura anómala\n{coma(y)} °C", (x, y), xytext=(-10, -4), textcoords="offset points",
                            ha="right", va="top", fontsize=8.6, color=TINTA)
        ax.set_title(titulo, fontsize=10, loc="left", color=TINTA)
        ax.set_xlabel("Tiempo desde el reinicio (min)")
        ax.set_xlim(0, max(t) + 0.4)
    ejes[0].set_ylabel("Temperatura del chip (°C)")
    ejes[0].set_ylim(lo, hi)
    guardar(fig, "fig3_temperatura.png")


def fig_latencia(filas):
    """filas: [(etiqueta, latencias en ms)], de abajo arriba."""
    fig, ax = plt.subplots(figsize=(9.2, 3.6))
    estilo(ax)
    ax.grid(axis="y", visible=False)
    limite = 400
    for y, (nombre, datos) in enumerate(filas):
        dentro = [d for d in datos if d <= limite]
        offs = [((k * 7) % 11 - 5) * 0.035 for k in range(len(dentro))]   # para que no se tapen
        ax.scatter(dentro, [y + o for o in offs], s=22, color=SERIE_1, edgecolors="white", linewidths=0.6, zorder=3)
        med = st.median(datos)
        ax.plot([med, med], [y - 0.3, y + 0.3], color=TINTA, lw=1.6, zorder=4)
        ax.text(med, y + 0.34, f"mediana {med:.0f} ms (n = {len(datos)})", ha="center", va="bottom",
                fontsize=8.6, color=TINTA)
        fuera = [d for d in datos if d > limite]
        if fuera:
            ax.annotate(f"{len(fuera)} más, hasta {coma(max(fuera) / 1000)} s →", (limite, y), xytext=(-4, -12),
                        textcoords="offset points", ha="right", va="center", fontsize=8.4, color=TINTA_2)
    ax.set_yticks(range(len(filas)), [f[0] for f in filas])
    ax.set_xlabel("Tiempo de un extremo al otro (ms), con la captura serie corregida")
    ax.set_xlim(0, limite)
    ax.set_ylim(-0.6, len(filas) - 0.2)
    guardar(fig, "fig4_latencia.png")


def bytes_mqtt_publish(topico, mensaje):
    """Paquete PUBLISH con QoS 0: cabecera fija (1 byte de tipo y la longitud
    restante), longitud del topico (2 bytes), topico y mensaje. Sin
    identificador de paquete: con QoS 0 no lleva."""
    resto = 2 + len(topico) + len(mensaje)
    return 1 + (1 if resto < 128 else 2) + resto


def bytes_http():
    """Peticion y respuesta medidas con curl (docs/evidencias/peticion_http.txt)."""
    texto = (EVID / "peticion_http.txt").read_text(encoding="utf-8")
    peticion = int(re.search(r"peticion=(\d+) B", texto)[1])
    cabeceras = int(re.search(r"cabeceras_respuesta=(\d+) B", texto)[1])
    cuerpo = int(re.search(r"cuerpo=(\d+) B", texto)[1])
    return peticion, cabeceras + cuerpo


def fig_bytes(mqtt_b, http_pet, http_resp):
    filas = [("HTTP · petición GET\n+ respuesta", http_pet + http_resp), ("MQTT · PUBLISH\n(QoS 0)", mqtt_b)]
    fig, ax = plt.subplots(figsize=(9.2, 2.4))
    estilo(ax)
    ax.grid(axis="y", visible=False)
    for y, (nombre, b) in enumerate(filas):
        ax.barh(y, b, height=0.38, color=SERIE_1)
        ax.text(b + 8, y, f"{b} B", va="center", fontsize=9.5, color=TINTA, fontweight="bold")
    ax.text(http_pet + http_resp + 82, 0, f"(petición {http_pet} B + respuesta {http_resp} B)",
            va="center", fontsize=8.6, color=TINTA_2)
    ax.set_yticks(range(len(filas)), [f[0] for f in filas])
    ax.set_xlabel("Bytes de aplicación por lectura enviada")
    ax.set_xlim(0, (http_pet + http_resp) * 1.75)
    ax.set_ylim(-0.6, len(filas) - 0.4)
    guardar(fig, "fig5_bytes.png")


# ------------------------------------------------------------------- main --

def main():
    # Publicador: primera version, version final y version final con la captura corregida
    v1 = datos_publicador("monitor_publicador_v1.txt", "pc_publicador_v1.txt")
    final = datos_publicador("monitor_publicador.txt", "pc_publicador.txt")
    corregida = datos_publicador("monitor_publicador_latencia.txt", "pc_publicador_latencia.txt")
    lat_v1 = informe_publicador("primera version", v1)
    lat_final = informe_publicador("version final", final)
    lat_pub = informe_publicador("version final, captura corregida", corregida)
    print("  ", resumen("latencia placa -> PC, primera version (captura antigua)", lat_v1))
    print("  ", resumen("latencia placa -> PC, version final (captura antigua)", lat_final))
    print("  ", resumen("latencia placa -> PC (captura corregida)", lat_pub))

    # Suscriptor: comandos con casos limite
    funcional = emparejar_por_contenido(recepciones_placa("monitor_suscriptor.txt"), envios_pc("pc_suscriptor.txt"))
    print("== Suscriptor, 26 comandos con casos limite")
    print(f"   llegaron {sum(1 for _, dt, _ in funcional if dt is not None)} de {len(funcional)}")
    for valor, dt, accion in funcional:
        print(f"   {valor!r:>12}  {'---' if dt is None else f'{dt:5.0f} ms'}  {accion}")
    alterna = [dt for v, dt, _ in funcional if dt is not None and v in ("0", "1")]
    print("  ", resumen("comandos 0/1 con ahorro de energia (captura antigua)", alterna))

    # Experimento: radio siempre encendida (misma captura antigua, comparable con la linea anterior)
    rec_on = recepciones_placa("monitor_suscriptor_sin_ahorro.txt")
    lat_on = por_entrada(envios_pc("pc_suscriptor_sin_ahorro.txt"), [(h, d) for h, d, _ in rec_on])
    print("  ", resumen("comandos 0/1 con la radio siempre encendida (captura antigua)", lat_on))
    print(f"   diferencia de medianas: {st.median(alterna) - st.median(lat_on):.0f} ms")

    # Captura corregida: PC -> placa y, para los mismos mensajes, PC -> PC
    envios = envios_pc("pc_suscriptor_latencia.txt")
    rec = recepciones_placa("monitor_suscriptor_latencia.txt")
    lat_sub = por_entrada(envios, [(h, d) for h, d, _ in rec])
    obs = [(h, t.split("  ", 1)[1]) for h, t in leer_pc("pc_observador_latencia.txt") if "/subscribe  " in t]
    lat_pc = por_entrada(envios, obs)
    print("== Captura corregida")
    print("  ", resumen("PC -> broker -> placa", lat_sub))
    print("  ", resumen("PC -> broker -> PC (referencia)", lat_pc))
    print("  ", resumen("placa -> broker -> PC", lat_pub))

    # Bytes por lectura
    mqtt_b = bytes_mqtt_publish("channels/esp32c6-e8f60afc9114/publish", "field1=29.80&status=MQTTPublish")
    mqtt_ts = bytes_mqtt_publish("channels/1234567/publish", "field1=29.80&status=MQTTPublish")
    http_pet, http_resp = bytes_http()
    print("== Bytes por lectura")
    print(f"   MQTT PUBLISH (este canal): {mqtt_b} B   (canal de ThingSpeak de 7 cifras: {mqtt_ts} B)")
    print(f"   HTTP: peticion {http_pet} B + respuesta {http_resp} B = {http_pet + http_resp} B  "
          f"-> MQTT usa el {100 * mqtt_b / (http_pet + http_resp):.1f} %")

    fig_arranque(final[0], final[1])
    fig_temperatura(v1, final)
    fig_latencia([("PC → broker → PC\n(referencia, sin la placa)", lat_pc),
                  ("PC → broker → placa\n(suscriptor)", lat_sub),
                  ("Placa → broker → PC\n(publicador)", lat_pub)])
    fig_bytes(mqtt_b, http_pet, http_resp)


if __name__ == "__main__":
    main()
