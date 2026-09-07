/*
 * EJEMPLO 1 - Concurrencia en un solo nucleo
 * ==========================================
 * Adaptacion fiel del primer ejemplo del material a Arduino-ESP32.
 *
 * Dos tareas de IGUAL prioridad (2) y un monitor de prioridad menor (1),
 * corriendo sobre el unico nucleo RISC-V del ESP32-C6.
 *
 *   vTareaLed     prio 2  - conmuta un LED cada 500 ms
 *   vTareaSensor  prio 2  - simula la lectura de un sensor cada 1000 ms
 *   vTareaMonitor prio 1  - late cada 5000 ms (equivale al while(1) de app_main)
 *
 * Lo que se quiere observar: las tres tareas parecen ejecutarse "a la vez"
 * aunque solo hay un motor de ejecucion. La ilusion la produce el cambio de
 * contexto del planificador, que ocurre cada vez que una tarea se bloquea en
 * vTaskDelay() y cede el nucleo.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "plataforma.h"
#include "ejemplos.h"

static const char *TAG = "SISTEMA";

/* GPIO 8: en la mayoria de DevKits del ESP32-C6 es el LED integrado.
 * Ojo: en varias placas ese LED es un WS2812 direccionable, por lo que el
 * material advierte que conviene conectar un LED fisico (con su resistencia
 * de 220-330 ohm) a este pin para ver el parpadeo. */
#ifndef BLINK_GPIO
#define BLINK_GPIO 8
#endif

/* TAREA 1: Parpadeo de un LED (simula control de perifericos) */
static void vTareaLed(void *pvParameters)
{
    (void)pvParameters;

    plat_led_init(BLINK_GPIO);

    uint8_t estado_led = 0;

    while (1) {
        estado_led = !estado_led;
        plat_led_write(BLINK_GPIO, estado_led);
        LOG_I("TAREA_LED", "Led cambiado a: %d", estado_led);

        /* Se bloquea 500 ms: aqui es donde el nucleo queda libre para que
         * corran la Tarea 2, el monitor y las tareas internas del sistema. */
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* TAREA 2: Lectura de un sensor ficticio (simula adquisicion de datos) */
static void vTareaSensor(void *pvParameters)
{
    (void)pvParameters;

    int contador_lecturas = 0;

    while (1) {
        contador_lecturas++;
        LOG_W("TAREA_SENSOR", "Leyendo sensor... Muestra #%d", contador_lecturas);

        /* 1000 ms. Al usar un periodo distinto al de la Tarea 1, el
         * planificador va alternando entre ambas sin saturar el nucleo. */
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* TAREA 3: Monitor de fondo. Cumple el papel del while(1) de app_main del
 * material. Se le da prioridad 1 (menor que las otras dos) precisamente
 * porque es la menos urgente. */
static void vTareaMonitor(void *pvParameters)
{
    (void)pvParameters;

    while (1) {
        LOG_I(TAG, "[Alerta] El sistema sigue vivo y estable.");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void ej1_arrancar(void)
{
    LOG_I(TAG, "Iniciando configuracion de tareas en el unico nucleo del ESP32-C6...");

    /* Tarea 1 - Prioridad 2 */
    xTaskCreate(
        vTareaLed,          /* Funcion que ejecuta la tarea       */
        "ControlLed",       /* Nombre identificativo (para debug) */
        2048,               /* Pila. OJO con la unidad: ESP-IDF la
                             * interpreta en BYTES, mientras que el
                             * FreeRTOS original la interpreta en
                             * PALABRAS. El mismo 2048 son 2 KB en el
                             * ESP32-C6 y 8 KB en el simulador de PC. */
        NULL,               /* Parametros de entrada              */
        2,                  /* Prioridad                          */
        NULL                /* Handle (no lo necesitamos aqui)    */
    );

    /* Tarea 2 - MISMA prioridad 2, para que compartan turno */
    xTaskCreate(
        vTareaSensor,
        "LecturaSensor",
        2048,
        NULL,
        2,
        NULL
    );

    /* Tarea 3 - Prioridad 1, monitor de fondo */
    xTaskCreate(
        vTareaMonitor,
        "Monitor",
        2048,
        NULL,
        1,
        NULL
    );

    LOG_I(TAG, "Tareas creadas. El planificador toma el control.");
}
