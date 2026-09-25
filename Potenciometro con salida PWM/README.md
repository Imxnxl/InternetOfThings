# Potenciómetro con salida PWM — ESP32‑C6

El ejercicio de clase: un potenciómetro en una **entrada analógica** (ADC, resolución por defecto
de 12 bits) y, con esa lectura, una **salida PWM** que regula el brillo de un LED.

```cpp
uint32_t mv    = analogReadMilliVolts(PIN_POT);      // lectura de 12 bits, calibrada en mV
uint32_t ciclo = min(mv, 3300) * 4095 / 3300;        // 0..3,3 V -> 0..4095
ledcWrite(PIN_LED_G, ciclo);                         // PWM de 5 kHz y 12 bits
```

**Resultado principal:** con la atenuación por defecto, el ADC del ESP32‑C6 **no** convierte
3,3 V en 4095. En la placa, el tope de la perilla (3,31 V) dio **3433 cuentas**. Pasando la
lectura cruda al PWM, el LED se quedaba en el **84 %** de su brillo. Por eso el ciclo de trabajo
se calcula con la tensión calibrada de fábrica, y el tope de la perilla da el 100 %. Los detalles
y las gráficas están en el informe.

> **[→ Leer el informe técnico](docs/INFORME_TECNICO.md)**
> · También en Word: [`docs/Informe_tecnico_Potenciometro_PWM_ESP32-C6.docx`](docs/Informe_tecnico_Potenciometro_PWM_ESP32-C6.docx)

---

## Conexiones

Materiales: placa ESP32‑C6, potenciómetro de 3 patas, módulo LED RGB. Sin resistencias.

| Componente | Pata | Pin de la placa |
|---|---|---|
| Potenciómetro | izquierda | **GND** |
| Potenciómetro | central (cursor) | **GPIO2** (ADC1, canal 2) |
| Potenciómetro | derecha | **3V3** (nunca 5V) |
| LED RGB | G | **GPIO20** (salida PWM) |
| LED RGB | R | GPIO19 (se deja apagado) |
| LED RGB | B | GPIO21 (se deja apagado) |
| LED RGB | común (−) | **GND** |

![Esquema](docs/evidencias/fig1_circuito.png)

- Potenciómetro mirado con la perilla hacia usted y las patas hacia abajo. Si al girar a la
  derecha el LED se atenúa en vez de aumentar, intercambie los cables de las patas de los extremos.
- En el ESP32‑C6 solo **GPIO0–GPIO6** tienen ADC. Se descartan GPIO4 y GPIO5 (pines de arranque)
  y GPIO0/GPIO1 (cristal opcional de 32 kHz); quedan GPIO2, GPIO3 y GPIO6.
- Conecte siempre por el **número de GPIO** serigrafiado: el orden físico de los pines cambia
  de un modelo de placa a otro.

---

## Qué hace el programa

1. Lee el cursor del potenciómetro con el ADC a 12 bits y atenuación de 12 dB (0–3,3 V en el C6):
   la lectura cruda (`analogRead`) y la tensión calibrada (`analogReadMilliVolts`).
2. Convierte la tensión (0–3300 mV) en el ciclo de trabajo de un PWM de **5 kHz y 12 bits**
   (0–4095) en el canal verde del LED (`ledcAttach` + `ledcWrite`). Con
   `USAR_TENSION_CALIBRADA = false` usa la lectura cruda tal cual, para comparar.
3. Cada 100 ms imprime `adc:… mV:… pwm_pct:…` por el monitor serie (115200 baudios). El formato lo
   entiende también el **Serial Plotter** de Arduino IDE.
4. Comandos en el monitor serie: **`r`** mide el ruido del ADC (con el potenciómetro quieto) y
   **`i`** muestra la configuración real (frecuencia PWM medida, fuerza de salida del pin).

---

## Grabarlo en la placa

Conecte el cable al conector **«UART»** de la placa (en Windows aparece como
*USB-Enhanced-SERIAL CH343*).

**Arduino IDE:** core «esp32» de Espressif 3.x · Placa **ESP32C6 Dev Module** (con *USB CDC On
Boot: Disabled*, lo predeterminado) · abrir `firmware/potenciometro_pwm/potenciometro_pwm.ino`
y subir.

**PlatformIO:**

```bash
cd firmware
pio run -t upload
pio device monitor
```

Si PlatformIO falla en Windows con `WindowsLongPathError`, vea la nota del README de la práctica
[`Pulsador con pull-down interno`](<../Pulsador con pull-down interno>).

---

## Estructura

```
├── firmware/
│   ├── platformio.ini
│   └── potenciometro_pwm/potenciometro_pwm.ino   El programa (Arduino IDE y PlatformIO)
├── docs/
│   ├── INFORME_TECNICO.md                        El informe
│   ├── Informe_tecnico_…ESP32-C6.docx            El mismo informe en Word
│   └── evidencias/                               Figuras, registros de la placa y fotos
└── scripts/           Figuras, gráficas de los datos de la placa y generación del .docx
```

---

## Licencia

MIT (ver `LICENSE` en la raíz del repositorio).
