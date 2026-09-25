/*
 * potenciometro_pwm.ino
 * ============================================================================
 * Practica: entrada analogica (potenciometro) -> salida PWM (brillo de un LED)
 * ESP32-C6
 * ============================================================================
 *
 * QUE HACE
 *   Lee la tension del cursor de un potenciometro con el ADC, con la
 *   resolucion por defecto de 12 bits, y con esa lectura fija el ciclo de
 *   trabajo de una senal PWM de 12 bits que regula el brillo de un LED:
 *
 *       cursor a 0 V    -> PWM   0 % -> LED apagado
 *       cursor a 1,65 V -> PWM  50 % -> LED a media potencia
 *       cursor a 3,3 V  -> PWM 100 % -> LED al maximo
 *
 * LO QUE SE APRENDIO EN LA PLACA
 *   La idea obvia es pasar la lectura cruda (0..4095) directamente al PWM
 *   (0..4095). Pero con la atenuacion de 12 dB, el ADC del ESP32-C6 no asigna
 *   4095 a 3,3 V: en la placa, el cursor a 3,31 V dio 3435 cuentas. Asi, el
 *   LED nunca pasaba del 84 % de brillo. Por eso el ciclo de trabajo se
 *   calcula con la tension calibrada de fabrica (analogReadMilliVolts,
 *   0..3300 mV). La version directa sigue disponible para comparar:
 *   USAR_TENSION_CALIBRADA = false.
 *
 * CONEXIONES
 *   Potenciometro  pata izquierda -> GND
 *                  pata central   -> GPIO2  (ADC1, canal 2)
 *                  pata derecha   -> 3V3    (NUNCA 5V: el C6 admite 3,6 V maximo)
 *
 *   LED RGB        G      -> GPIO20 (PWM)
 *                  R      -> GPIO19 (apagado)
 *                  B      -> GPIO21 (apagado)
 *                  comun  -> GND  (catodo comun; ver LED_ANODO_COMUN)
 *
 *   Por que GPIO2: en el ESP32-C6 solo GPIO0..GPIO6 llegan al ADC. GPIO4 y
 *   GPIO5 son pines de arranque (strapping) y GPIO0/GPIO1 son los del
 *   cristal opcional de 32 kHz, asi que quedan GPIO2, GPIO3 y GPIO6.
 *
 * MONITOR SERIE (115200 baudios)
 *   Cada 100 ms una linea "adc:... mV:... pwm_pct:..." que tambien entiende
 *   el Serial Plotter de Arduino IDE. Comandos (escribirlos en el monitor):
 *     r  prueba de ruido del ADC (con el potenciometro quieto)
 *     i  vuelve a mostrar la configuracion
 */
#include <Arduino.h>
#include "driver/gpio.h"   // API de ESP-IDF: fuerza de salida y lectura de la configuracion real

/* ------------------------------------------------------------- Pines -- */
const uint8_t PIN_POT   = 2;
const uint8_t PIN_LED_R = 19;
const uint8_t PIN_LED_G = 20;
const uint8_t PIN_LED_B = 21;

/* ------------------------------------------------------ Configuracion -- */

// Resolucion del ADC: 12 bits es la de por defecto (y la maxima) del C6.
const uint8_t  ADC_BITS = 12;
const uint32_t ADC_MAX  = (1UL << ADC_BITS) - 1;   // 4095

// PWM: la misma resolucion que el ADC (0..4095). 5 kHz queda muy por encima
// de lo que el ojo percibe como parpadeo.
const uint32_t PWM_FRECUENCIA_HZ = 5000;
const uint8_t  PWM_BITS          = ADC_BITS;
const uint32_t PWM_MAX           = (1UL << PWM_BITS) - 1;   // 4095

// Como se calcula el ciclo de trabajo:
//   true  -> a partir de la tension calibrada: 0..3300 mV -> 0..4095.
//            El tope de la perilla da el 100 % (recomendado).
//   false -> la lectura cruda tal cual: 0..4095 -> 0..4095. Es lo mas
//            simple, pero en el C6 el tope de la perilla solo llega a ~3435
//            cuentas, es decir, al ~84 % de brillo.
const bool     USAR_TENSION_CALIBRADA = true;
const uint32_t TENSION_MAXIMA_MV      = 3300;   // tension que da el 100 %

// Tipo de LED RGB. En uno de catodo comun el color se enciende con HIGH;
// en uno de anodo comun, con LOW, y el ciclo de trabajo hay que invertirlo.
const bool LED_ANODO_COMUN = false;

// Canales que se regulan con el potenciometro (true = ese color se usa).
const bool COLOR_R = false;
const bool COLOR_G = true;
const bool COLOR_B = false;

// Fuerza de salida de los pines del LED (igual que en la practica del
// pulsador): segun el manual tecnico del C6, nivel 0 ~5 mA, 1 ~10 mA,
// 2 ~20 mA (por defecto) y 3 ~40 mA. El nivel 0 limita la corriente por si
// el LED no lleva resistencia en serie.
const gpio_drive_cap_t FUERZA_SALIDA_LED = GPIO_DRIVE_CAP_0;

// Cada cuanto se informa por el monitor serie.
const uint32_t PERIODO_INFORME_MS = 100;

// Resultado de la prueba de ruido (en cuentas del ADC). Se declara antes de
// cualquier funcion: Arduino IDE y PlatformIO generan los prototipos de las
// funciones del .ino al principio, y el tipo tiene que existir ya ahi.
struct Estadistica { double media, desviacion, minimo, maximo; };

/* ----------------------------------------------------------------- LED -- */

const uint8_t PINES_LED[] = { PIN_LED_R, PIN_LED_G, PIN_LED_B };
const bool    USAR_LED[]  = { COLOR_R,   COLOR_G,   COLOR_B };

// Frecuencia que ha conseguido de verdad el temporizador del PWM (ver setup).
uint32_t frecuenciaRealHz = 0;

// Escribe el mismo ciclo de trabajo en los canales en uso. Aqui, y solo
// aqui, se invierte para el anodo comun: el resto del programa trabaja con
// "0 = apagado, PWM_MAX = maximo brillo".
void escribirBrillo(uint32_t ciclo)
{
    uint32_t salida = LED_ANODO_COMUN ? PWM_MAX - ciclo : ciclo;
    for (int i = 0; i < 3; i++) {
        if (USAR_LED[i]) ledcWrite(PINES_LED[i], salida);
    }
}

/* ------------------------------------------------------- Diagnostico -- */

// Muestra la configuracion tal como ha quedado en el hardware: la frecuencia
// real del PWM y la fuerza de salida leida de los registros del chip
// (gpio_get_io_config).
void informarConfiguracion()
{
    gpio_io_config_t led;
    gpio_get_io_config((gpio_num_t)PIN_LED_G, &led);

    Serial.println();
    Serial.println("==========================================================");
    Serial.println(" Potenciometro (ADC) -> brillo de un LED (PWM) - ESP32-C6");
    Serial.printf (" %s rev %d | %lu MHz | Arduino-ESP32 %d.%d.%d\n",
                   ESP.getChipModel(), (int)ESP.getChipRevision(),
                   (unsigned long)getCpuFrequencyMhz(),
                   ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR,
                   ESP_ARDUINO_VERSION_PATCH);
    Serial.println("==========================================================");
    Serial.printf("Entrada  GPIO%u -> ADC1 canal %d | %u bits (0..%lu) | atenuacion 12 dB (0..3,3 V)\n",
                  PIN_POT, (int)digitalPinToAnalogChannel(PIN_POT), ADC_BITS,
                  (unsigned long)ADC_MAX);
    for (int i = 0; i < 3; i++) {
        if (!USAR_LED[i]) continue;
        Serial.printf("Salida   GPIO%u -> PWM %lu Hz pedidos, %lu Hz reales | %u bits (0..%lu)\n",
                      PINES_LED[i], (unsigned long)PWM_FRECUENCIA_HZ,
                      (unsigned long)frecuenciaRealHz, PWM_BITS,
                      (unsigned long)PWM_MAX);
    }
    if (USAR_TENSION_CALIBRADA)
        Serial.printf("Ciclo    tension calibrada: 0..%lu mV -> 0..%lu (el tope de la perilla da el 100 %%)\n",
                      (unsigned long)TENSION_MAXIMA_MV, (unsigned long)PWM_MAX);
    else
        Serial.printf("Ciclo    lectura cruda tal cual: 0..%lu -> 0..%lu\n",
                      (unsigned long)ADC_MAX, (unsigned long)PWM_MAX);
    Serial.printf("         LED de %s comun | fuerza de salida: nivel %d de 3 (registros del chip)\n",
                  LED_ANODO_COMUN ? "anodo" : "catodo", (int)led.drv);
    Serial.println("Comandos: r = prueba de ruido del ADC, i = esta cabecera");
    Serial.println("----------------------------------------------------------");
}

// Mide n veces la entrada, promediando "grupo" lecturas en cada medida, y
// devuelve media, desviacion tipica, minimo y maximo (en cuentas del ADC).
Estadistica medirEntrada(uint16_t n, uint8_t grupo)
{
    double suma = 0, sumaCuadrados = 0, minimo = 1e9, maximo = -1;
    for (uint16_t i = 0; i < n; i++) {
        uint32_t acumulado = 0;
        for (uint8_t k = 0; k < grupo; k++) acumulado += analogRead(PIN_POT);
        double v = (double)acumulado / grupo;
        suma += v;
        sumaCuadrados += v * v;
        if (v < minimo) minimo = v;
        if (v > maximo) maximo = v;
    }
    double media = suma / n;
    double varianza = sumaCuadrados / n - media * media;
    return { media, varianza > 0 ? sqrt(varianza) : 0, minimo, maximo };
}

// Prueba de ruido: con el potenciometro quieto, la lectura deberia ser
// siempre la misma. La dispersion que aparezca es ruido del ADC y del
// montaje. Se mide con lecturas sueltas (como hace loop()) y con promedios
// de 16, que es lo que recomienda la hoja de datos para mejorar la precision.
void pruebaRuido()
{
    Serial.println("--- Prueba de ruido del ADC (no mueva el potenciometro) ---");
    struct { uint16_t n; uint8_t grupo; const char *nombre; } casos[] = {
        { 1000, 1,  "1000 lecturas sueltas   " },
        {  200, 16, " 200 promedios de 16    " },
    };
    for (auto &c : casos) {
        Estadistica e = medirEntrada(c.n, c.grupo);
        Serial.printf("%s media %7.1f | min %6.1f | max %6.1f | rango %5.1f | desv. tipica %5.2f cuentas\n",
                      c.nombre, e.media, e.minimo, e.maximo, e.maximo - e.minimo,
                      e.desviacion);
    }
    Serial.println("-----------------------------------------------------------");
}

/* ------------------------------------------------------ setup / loop -- */

void setup()
{
    Serial.begin(115200);
    uint32_t inicio = millis();
    while (!Serial && millis() - inicio < 3000) delay(10);

    // ENTRADA ANALOGICA. Son los valores por defecto del core; se fijan de
    // forma explicita para que el programa no dependa de ellos.
    analogReadResolution(ADC_BITS);
    analogSetAttenuation(ADC_11db);   // 12 dB en ESP-IDF 5.x: rango 0..3,3 V en el C6

    // SALIDA PWM en los canales en uso; los demas, apagados.
    for (int i = 0; i < 3; i++) {
        if (USAR_LED[i]) {
            ledcAttach(PINES_LED[i], PWM_FRECUENCIA_HZ, PWM_BITS);
            // ledcReadFreq() devuelve 0 mientras el ciclo de trabajo es 0 %
            // (asi esta programado en el core 3.3), y al arrancar todavia no
            // se ha escrito ninguno. ledcChangeFrequency() reaplica la misma
            // configuracion y devuelve la frecuencia real del temporizador.
            frecuenciaRealHz = ledcChangeFrequency(PINES_LED[i], PWM_FRECUENCIA_HZ, PWM_BITS);
        } else {
            pinMode(PINES_LED[i], OUTPUT);
            digitalWrite(PINES_LED[i], LED_ANODO_COMUN ? HIGH : LOW);
        }
        // Despues de configurar el pin, que deja la fuerza por defecto.
        gpio_set_drive_capability((gpio_num_t)PINES_LED[i], FUERZA_SALIDA_LED);
    }

    informarConfiguracion();
}

void loop()
{
    // 1) Leer la entrada analogica (12 bits): la lectura cruda, que en el C6
    //    va de 0 a ~3435, y la tension corregida con la calibracion de fabrica.
    uint16_t lectura     = analogRead(PIN_POT);
    uint32_t milivoltios = analogReadMilliVolts(PIN_POT);

    // 2) Calcular el ciclo de trabajo y mandarlo a la salida PWM.
    uint32_t ciclo;
    if (USAR_TENSION_CALIBRADA) {
        uint32_t mv = min(milivoltios, TENSION_MAXIMA_MV);
        ciclo = mv * PWM_MAX / TENSION_MAXIMA_MV;
    } else {
        ciclo = lectura;
    }
    escribirBrillo(ciclo);

    // 3) Informar cada PERIODO_INFORME_MS.
    static uint32_t ultimoInforme = 0;
    if (millis() - ultimoInforme >= PERIODO_INFORME_MS) {
        ultimoInforme = millis();
        Serial.printf("adc:%u mV:%lu pwm_pct:%.1f\n", lectura,
                      (unsigned long)milivoltios, ciclo * 100.0 / PWM_MAX);
    }

    if (Serial.available()) {
        char c = Serial.read();
        if (c == 'r' || c == 'R') pruebaRuido();
        if (c == 'i' || c == 'I') informarConfiguracion();
    }

    // 4) Ceder el nucleo: loop() es una tarea de FreeRTOS (practica de
    //    multitareas). 5 ms dan 200 actualizaciones del LED por segundo.
    delay(5);
}
