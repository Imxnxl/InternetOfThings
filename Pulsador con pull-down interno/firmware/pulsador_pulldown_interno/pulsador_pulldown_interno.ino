/*
 * pulsador_pulldown_interno.ino
 * ============================================================================
 * Practica: entrada digital con resistencia pull-down INTERNA - ESP32-C6
 * ============================================================================
 *
 * QUE HACE
 *   Lee un pulsador en una entrada digital y copia su estado en una salida
 *   digital que enciende un LED:
 *
 *       pulsador presionado -> la entrada lee HIGH -> LED encendido
 *       pulsador suelto     -> la entrada lee LOW  -> LED apagado
 *
 * DIFERENCIA CON EL EJERCICIO DE CLASE
 *   En clase, el pull-down era una resistencia fisica (10 kohm) entre el pin
 *   y GND. Aqui no hay ninguna resistencia en la protoboard: se activa la que
 *   el ESP32-C6 lleva integrada en cada GPIO (45 kohm tipicos, hoja de datos
 *   del ESP32-C6, tabla 5-4). Todo el cambio esta en pinMode():
 *
 *       pinMode(PIN_BOTON, INPUT);            // clase: pull-down externo
 *       pinMode(PIN_BOTON, INPUT_PULLDOWN);   // esta practica: interno
 *
 * CONEXIONES (ESP32-C6-DevKitC-1)
 *
 *   Pulsador   pata 1 -> GPIO18
 *              pata 2 -> 3V3        (NUNCA a 5V: el C6 admite 3,6 V maximo)
 *
 *   LED RGB    R      -> GPIO19
 *              G      -> GPIO20
 *              B      -> GPIO21
 *              comun  -> GND   si es de catodo comun (lo habitual)
 *                     -> 3V3   si es de anodo comun (ver LED_ANODO_COMUN)
 *
 *   Por que estos pines: no son de arranque (strapping: 4, 5, 8, 9 y 15), ni
 *   del USB (12 y 13), ni de la UART de programacion (16 y 17), y estan
 *   seguidos en el conector J3 de la placa. El GPIO8, ademas, lleva el LED
 *   direccionable (WS2812) integrado en la placa.
 *
 * MONITOR SERIE (115200 baudios)
 *   Al arrancar muestra la configuracion real de los pines, leida de los
 *   registros del chip, y despues una linea por cada pulsacion. Enviando la
 *   letra 'i' se vuelve a mostrar la configuracion.
 */
#include <Arduino.h>
#include "driver/gpio.h"   // API de ESP-IDF: fuerza de salida y lectura de la configuracion real

/* ------------------------------------------------------------- Pines -- */
const uint8_t PIN_BOTON = 18;
const uint8_t PIN_LED_R = 19;
const uint8_t PIN_LED_G = 20;
const uint8_t PIN_LED_B = 21;

/* ------------------------------------------------------ Configuracion -- */

// 1 = la practica: INPUT_PULLDOWN.
// 0 = experimento: INPUT a secas, sin ninguna resistencia, para observar
//     una entrada flotante. En PlatformIO se elige con el entorno
//     (pio run -e sin_pulldown); en Arduino IDE, cambiando este valor.
#ifndef USAR_PULLDOWN_INTERNO
#define USAR_PULLDOWN_INTERNO 1
#endif

// Tipo de LED RGB. En uno de catodo comun cada color se enciende con HIGH;
// en uno de anodo comun, con LOW. Si el LED se ve encendido con el pulsador
// suelto y se apaga al presionarlo, es de anodo comun: ponga true.
const bool LED_ANODO_COMUN = false;

// Color que se enciende al presionar el pulsador (true = canal encendido).
const bool COLOR_R = false;
const bool COLOR_G = true;
const bool COLOR_B = false;

// Fuerza de salida de los pines del LED. Segun el manual tecnico del C6
// (registro IO_MUX_GPIOn_REG, campo FUN_DRV): nivel 0 ~5 mA, 1 ~10 mA,
// 2 ~20 mA (el de por defecto) y 3 ~40 mA. El nivel 0 reduce la corriente
// para poder conectar el LED sin resistencia en serie. No sustituye a una
// resistencia de 220-330 ohm: si dispone de ellas, pongalas y puede volver
// a GPIO_DRIVE_CAP_2.
const gpio_drive_cap_t FUERZA_SALIDA_LED = GPIO_DRIVE_CAP_0;

// Un pulsador mecanico "rebota": al cerrarse abre y cierra varias veces en
// pocos milisegundos. Un cambio solo se da por bueno si la lectura se
// mantiene estable este tiempo. Se usa unicamente para el registro serie:
// el LED copia la entrada tal cual, como en el ejercicio de clase.
const uint32_t ANTIRREBOTE_MS = 30;

/* ------------------------------------------- Estado del registro serie -- */
int      ultimaLectura       = LOW;  // ultima lectura en bruto
int      estadoEstable       = LOW;  // estado confirmado tras el antirrebote
uint32_t instanteUltimoCambio = 0;   // ms del ultimo cambio en bruto
uint32_t instantePrimerCambio = 0;   // ms del primer cambio de la transicion en curso
uint32_t cambiosLeidos       = 0;    // cambios en bruto desde el ultimo evento
uint32_t pulsaciones         = 0;
uint32_t instantePulsacion   = 0;

/* ----------------------------------------------------------------- LED -- */

// Enciende o apaga un canal. Aqui, y solo aqui, se resuelve la logica
// invertida del anodo comun: el resto del programa piensa en encendido/apagado.
void escribirCanal(uint8_t pin, bool encender)
{
    bool nivelAlto = LED_ANODO_COMUN ? !encender : encender;
    digitalWrite(pin, nivelAlto ? HIGH : LOW);
}

void escribirLed(bool encendido)
{
    escribirCanal(PIN_LED_R, encendido && COLOR_R);
    escribirCanal(PIN_LED_G, encendido && COLOR_G);
    escribirCanal(PIN_LED_B, encendido && COLOR_B);
}

// Enciende rojo, verde y azul por turnos. Sirve para comprobar el cableado
// de los tres canales y el tipo de LED antes de empezar.
void probarLed()
{
    const uint8_t pines[]   = { PIN_LED_R, PIN_LED_G, PIN_LED_B };
    const char   *nombres[] = { "rojo", "verde", "azul" };

    Serial.print("Prueba del LED:");
    for (int i = 0; i < 3; i++) {
        Serial.printf(" %s...", nombres[i]);
        escribirCanal(pines[i], true);
        delay(400);
        escribirCanal(pines[i], false);
    }
    Serial.println(" ok");
}

/* ------------------------------------------------------- Diagnostico -- */

// Muestra la configuracion de los pines tal como la tiene el hardware. Se
// lee de los registros del chip con gpio_get_io_config(), asi que no repite
// lo que el programa cree haber configurado: comprueba que se ha aplicado.
void informarConfiguracion()
{
    gpio_io_config_t boton, led;
    gpio_get_io_config((gpio_num_t)PIN_BOTON, &boton);
    gpio_get_io_config((gpio_num_t)PIN_LED_R, &led);

    Serial.println();
    Serial.println("==========================================================");
    Serial.println(" Pulsador con resistencia pull-down INTERNA - ESP32-C6");
    Serial.printf (" %s rev %d | %lu MHz | Arduino-ESP32 %d.%d.%d\n",
                   ESP.getChipModel(), (int)ESP.getChipRevision(),
                   (unsigned long)getCpuFrequencyMhz(),
                   ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR,
                   ESP_ARDUINO_VERSION_PATCH);
    Serial.println("==========================================================");

#if USAR_PULLDOWN_INTERNO
    Serial.printf("Entrada  GPIO%u  pinMode INPUT_PULLDOWN\n", PIN_BOTON);
#else
    Serial.printf("Entrada  GPIO%u  pinMode INPUT (sin pull-down)\n", PIN_BOTON);
    Serial.println("         *** EXPERIMENTO: entrada flotante, lecturas impredecibles ***");
#endif
    Serial.printf("         registros del chip -> pull-down: %s | pull-up: %s | entrada: %s\n",
                  boton.pd ? "ACTIVADO" : "no", boton.pu ? "ACTIVADO" : "no",
                  boton.ie ? "si" : "no");

    Serial.printf("Salidas  GPIO%u (R)  GPIO%u (G)  GPIO%u (B)  LED de %s comun\n",
                  PIN_LED_R, PIN_LED_G, PIN_LED_B, LED_ANODO_COMUN ? "anodo" : "catodo");
    Serial.printf("         registros del chip -> salida: %s | fuerza: nivel %d de 3\n",
                  led.oe ? "si" : "no", (int)led.drv);

    Serial.printf("Lectura actual de la entrada: %d  (con el pulsador suelto debe ser 0)\n",
                  digitalRead(PIN_BOTON));
    Serial.println("----------------------------------------------------------");
}

// Escribe una linea por cada pulsacion y cada liberacion confirmadas.
// "cambios leidos" cuenta las veces que la entrada cambio de valor en bruto
// durante la transicion: 1 si fue limpia, mas de 1 si el contacto reboto.
void registrarCambios(int lectura)
{
    uint32_t ahora = millis();

    if (lectura != ultimaLectura) {
        if (cambiosLeidos == 0) instantePrimerCambio = ahora;
        ultimaLectura        = lectura;
        instanteUltimoCambio = ahora;
        cambiosLeidos++;
    }

    // Nada que informar hasta que la lectura lleve ANTIRREBOTE_MS sin moverse.
    if (cambiosLeidos == 0 || ahora - instanteUltimoCambio < ANTIRREBOTE_MS) return;

    if (lectura == estadoEstable) {
        // Volvio al estado de partida antes de estabilizarse: un toque muy
        // breve o, sin pull-down, ruido en una entrada flotante.
        Serial.printf("[%8lu ms] pulso de menos de %lu ms, descartado     | cambios leidos: %lu\n",
                      (unsigned long)instantePrimerCambio, (unsigned long)ANTIRREBOTE_MS,
                      (unsigned long)cambiosLeidos);
    } else if (lectura == HIGH) {
        estadoEstable = HIGH;
        pulsaciones++;
        instantePulsacion = instantePrimerCambio;
        Serial.printf("[%8lu ms] PRESIONADO -> entrada HIGH, LED encendido | pulsacion #%lu | cambios leidos: %lu\n",
                      (unsigned long)instantePrimerCambio, (unsigned long)pulsaciones,
                      (unsigned long)cambiosLeidos);
    } else {
        estadoEstable = LOW;
        Serial.printf("[%8lu ms] SUELTO     -> entrada LOW,  LED apagado   | duracion: %lu ms | cambios leidos: %lu\n",
                      (unsigned long)instantePrimerCambio,
                      (unsigned long)(instantePrimerCambio - instantePulsacion),
                      (unsigned long)cambiosLeidos);
    }
    cambiosLeidos = 0;
}

/* ------------------------------------------------------ setup / loop -- */

void setup()
{
    Serial.begin(115200);

    // Con el USB nativo se da hasta 3 s para que el PC abra el puerto; asi no
    // se pierde la cabecera. Por la UART, !Serial es siempre falso y no espera.
    uint32_t inicio = millis();
    while (!Serial && millis() - inicio < 3000) delay(10);

    // ENTRADA: el unico cambio respecto al codigo de clase.
#if USAR_PULLDOWN_INTERNO
    pinMode(PIN_BOTON, INPUT_PULLDOWN);
#else
    pinMode(PIN_BOTON, INPUT);
#endif

    // SALIDAS: los tres canales del LED RGB.
    const uint8_t pinesLed[] = { PIN_LED_R, PIN_LED_G, PIN_LED_B };
    for (uint8_t pin : pinesLed) {
        pinMode(pin, OUTPUT);
        // Despues de pinMode(), que deja la fuerza por defecto.
        gpio_set_drive_capability((gpio_num_t)pin, FUERZA_SALIDA_LED);
    }
    escribirLed(false);

    informarConfiguracion();
    probarLed();

    ultimaLectura = estadoEstable = digitalRead(PIN_BOTON);
    Serial.println("Listo. Presione el pulsador.");
}

void loop()
{
    // 1) Leer el estado del pulsador en la entrada digital.
    int lectura = digitalRead(PIN_BOTON);

    // 2) Mandar ese estado a la salida digital que enciende el LED.
    escribirLed(lectura == HIGH);

    // 3) Informar por el monitor serie (no influye en el LED).
    registrarCambios(lectura);

    if (Serial.available()) {
        char c = Serial.read();
        if (c == 'i' || c == 'I') informarConfiguracion();
    }

    // 4) Ceder el nucleo 1 ms. loop() es una tarea de FreeRTOS: si no se
    //    bloquea nunca, deja sin CPU a la tarea Idle (practica de multitareas).
    //    Leer cada 1 ms es mas que suficiente para un pulsador.
    delay(1);
}
