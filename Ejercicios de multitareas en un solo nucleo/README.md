# Ejercicios de multitareas en un solo núcleo — ESP32‑C6

Práctica sobre planificación de tareas con FreeRTOS en un microcontrolador de un
solo núcleo, a partir del material *«En un ESP32 de un solo núcleo, xTaskCreate
funciona mediante multitarea por tiempo compartido (time‑slicing) y un sistema de
prioridades gestionado por el sistema operativo FreeRTOS»*.

Contiene los **dos ejemplos del material** reproducidos íntegramente, **tres
experimentos añadidos** para verificar de forma medible lo que el material afirma
en prosa, y el **informe técnico** con los resultados.

> **[→ Leer el informe técnico](docs/INFORME_TECNICO.md)**
> · También disponible en Word: [`docs/Informe_tecnico_FreeRTOS_ESP32-C6.docx`](docs/Informe_tecnico_FreeRTOS_ESP32-C6.docx) (19 páginas)

---

## La idea: un solo código, dos destinos

El mismo código de las tareas se compila para la placa y para el PC. Solo cambia
la implementación de cuatro funciones de la capa de abstracción:

```
shared/ejemplos_freertos/src/       ← los 5 ejemplos (una sola copia)
        │
        ├──► firmware/     Arduino-ESP32 → ESP32-C6 real
        └──► simulacion/   kernel real de FreeRTOS (port MSVC-MingW) → PC
```

La carpeta `simulacion/` **no imita** a FreeRTOS: enlaza el código fuente oficial
del kernel (`tasks.c`, `queue.c`, `list.c`, `timers.c`) y el port de Windows, que
emula un microcontrolador de un solo núcleo. Las trazas de las que salen todas
las medidas del informe las emite el propio kernel desde `vTaskSwitchContext()`.

---

## Los cinco experimentos

| # | Qué demuestra | Origen |
|---|---|---|
| 1 | Tres tareas concurrentes sobre un núcleo: LED cada 500 ms, sensor cada 1000 ms | **Ejemplo del material** |
| 2 | Cola productor/consumidor: espera bloqueada a 0 % de CPU | **Ejemplo del material** |
| 3 | Inanición y disparo del Task Watchdog Timer | Añadido |
| 4 | Preemción inmediata por prioridad, con la CPU saturada | Añadido |
| 5 | Reparto por turnos (*time‑slicing*) entre tareas de igual prioridad | Añadido |

Resultados resumidos: latencia de cola **0 ms**, latencia de preemción **0 ticks**
con el núcleo al 100 %, reparto **49,99 / 50,01 %** entre iguales, y watchdog
disparándose a los **9902 ms** en el experimento de inanición.

---

## Estructura

```
├── material/          Nota sobre el documento de partida (no redistribuido)
├── docs/
│   ├── INFORME_TECNICO.md      El informe (desarrollo, resultados, conclusiones)
│   ├── Informe_...C6.docx      El mismo informe en Word, generado del anterior
│   └── evidencias/             Capturas de consola y figuras
├── shared/
│   └── ejemplos_freertos/src/  Los 5 ejemplos + la capa de abstracción
├── firmware/          Proyecto PlatformIO para el ESP32-C6 (Arduino)
├── simulacion/        Simulador de PC contra el kernel real de FreeRTOS
│   └── salidas/       Logs de consola y trazas CSV de cada ejecución
└── scripts/           Generación de figuras y capturas a partir de las trazas
```

---

## Reproducir los resultados

### Requisitos

Dos dependencias, que se instalan en un `tools/` **en la raíz del repositorio** y
no tocan el sistema. Desde la raíz:

```bash
# 1. Kernel de FreeRTOS
git clone --depth 1 --branch V11.1.0 \
    https://github.com/FreeRTOS/FreeRTOS-Kernel.git tools/FreeRTOS-Kernel

# 2. Compilador MinGW-w64 (si no tiene ya gcc)
#    Descargue el .zip de https://github.com/brechtsanders/winlibs_mingw/releases
#    y descomprímalo en tools/ , de modo que quede tools/mingw64/bin/gcc.exe
```

> **La ruta de `tools/` no puede contener espacios.** MinGW no funciona si su
> propia ruta de instalación los tiene: `ld.exe` la parte por el espacio y no
> encuentra sus librerías. Por eso `tools/` vive en la raíz del repositorio y no
> dentro de esta carpeta, que sí los tiene. Los archivos del proyecto sí pueden
> estar bajo una ruta con espacios; el problema es solo la del compilador.
>
> `compilar.sh` busca `tools/` subiendo hasta cuatro niveles. Si la tiene en otro
> sitio, indíqueselo con `TOOLS=/ruta/a/tools`.

Para las figuras: `pip install matplotlib pillow`.

### Ejecución

```bash
bash simulacion/compilar.sh          # compila el simulador
bash simulacion/ejecutar_todo.sh     # ejecuta los 5 experimentos
python scripts/generar_figuras.py    # regenera las figuras del informe
python scripts/capturas_consola.py   # regenera las capturas de consola
```

Un experimento suelto, indicando número y segundos de tiempo simulado:

```bash
simulacion/build/simulador.exe 3 20 simulacion/salidas
```

Si `gcc` ya está en el `PATH`, o el kernel está en otro sitio:

```bash
GCC=gcc KERNEL=/ruta/a/FreeRTOS-Kernel bash simulacion/compilar.sh
```

Las rutas de los comandos son relativas a esta carpeta.

---

## Grabar el firmware en una placa ESP32‑C6

```bash
cd firmware
pio run -e ejemplo1 -t upload     # ejemplo1 ... ejemplo5
pio device monitor -b 115200
```

Dos avisos:

- **La plataforma oficial `espressif32` de PlatformIO no vale**: define la placa
  `esp32-c6-devkitc-1` solo para ESP‑IDF, y su core de Arduino sigue en la serie
  2.x, sin soporte de C6. `platformio.ini` usa por eso el fork *pioarduino*, que
  empaqueta el core Arduino 3.x. La alternativa es Arduino IDE con el core
  oficial «esp32 by Espressif Systems» 3.x.
- **El LED del GPIO 8**: en muchos DevKit del C6 ese pin lleva un LED RGB
  direccionable (WS2812), no un LED simple. Como advierte el material, conecte un
  LED normal con su resistencia de 220–330 Ω a ese pin para ver el parpadeo, o
  cambie el pin con `-DBLINK_GPIO=<n>`.

El firmware **no se ha llegado a grabar en la placa**: la ESP32‑C6 todavía no tiene
soldadas las tiras de pines, no se consiguió ayuda para soldarla a tiempo, y sin
ellas no se puede montar en protoboard ni conectar el LED del ejemplo 1. Por eso
todos los resultados del informe proceden de la simulación contra el kernel real
de FreeRTOS. La sección 7 del informe detalla con precisión qué se traslada al
chip y qué no.

---

## Licencia

MIT para el código de esta carpeta (ver `LICENSE` en la raíz del repositorio).
FreeRTOS es MIT, propiedad de Amazon Web Services.

El documento de partida **no se incluye**: conserva la titularidad de su autor
(ver `material/LEEME.md`). El informe es autocontenido y puede leerse sin él.
