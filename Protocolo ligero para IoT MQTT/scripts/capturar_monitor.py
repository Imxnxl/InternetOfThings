"""
capturar_monitor.py - Guarda la salida serie de la placa en un fichero de texto.

Reinicia el ESP32-C6 (linea RTS -> EN, igual que hace idf.py monitor), lee el
puerto durante los segundos indicados y escribe cada linea con la hora del PC
(en milisegundos) y el tiempo transcurrido desde el reinicio. Quita los
codigos de color de ESP_LOG.

La hora del PC permite comparar este registro con el de scripts/cliente_mqtt.js
(que usa el mismo reloj) y medir cuanto tarda un mensaje en cruzar el broker.

Uso (con el entorno de ESP-IDF activo, que ya trae pyserial):
    python scripts/capturar_monitor.py COM5 90 docs/evidencias/monitor_publicador.txt
    python scripts/capturar_monitor.py COM5 90 salida.txt --sin-reset
"""
import re
import sys
import time
from datetime import datetime, timezone

import serial

COLOR_ANSI = re.compile(r"\x1b\[[0-9;]*m")


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        sys.exit(1)
    puerto, segundos, salida = sys.argv[1], float(sys.argv[2]), sys.argv[3]
    reiniciar = "--sin-reset" not in sys.argv

    s = serial.Serial()
    s.port = puerto
    s.baudrate = 115200
    s.timeout = 0.1
    # DTR va a GPIO9 (modo de arranque) y RTS a EN (reset) a traves de los
    # transistores de la placa. Ambas en reposo al abrir el puerto, para que
    # abrirlo no deje la placa en modo de descarga.
    s.dtr = False
    s.rts = False
    s.open()
    if reiniciar:
        s.rts = True        # EN a nivel bajo: chip en reset
        time.sleep(0.2)
        s.rts = False       # EN a nivel alto: arranca el programa grabado

    t0 = time.time()
    pendiente = b""
    with open(salida, "w", encoding="utf-8", newline="\n") as f:
        while time.time() - t0 < segundos:
            # Se pide lo que ya ha llegado (al menos 1 byte). Con read(2048), la
            # lectura esperaba a llenar el buffer o a agotar el timeout, y cada
            # linea se sellaba con un retraso de ~100 ms (seccion 5.4 del informe).
            pendiente += s.read(max(1, s.in_waiting))
            while b"\n" in pendiente:
                linea, pendiente = pendiente.split(b"\n", 1)
                texto = COLOR_ANSI.sub("", linea.decode("utf-8", errors="replace")).rstrip("\r")
                # UTC, en el mismo formato que toISOString() de cliente_mqtt.js
                hora = datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")
                registro = f"[{hora} | {time.time() - t0:8.3f} s] {texto}"
                f.write(registro + "\n")
                f.flush()
                print(registro, flush=True)
    s.close()


if __name__ == "__main__":
    main()
