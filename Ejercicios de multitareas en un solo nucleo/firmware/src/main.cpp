/*
 * main.cpp - Arranque del firmware en el ESP32-C6 (framework Arduino)
 *
 * En Arduino-ESP32, setup() y loop() ya se ejecutan dentro de una tarea de
 * FreeRTOS llamada "loopTask" (prioridad 1). Es decir: cuando llamamos a
 * xTaskCreate() desde setup() no estamos "activando" FreeRTOS, que ya lleva
 * corriendo desde antes; solo anadimos tareas nuevas al planificador.
 *
 * Ese detalle importa para leer bien los ejemplos: nuestras tareas de
 * prioridad 2 son MAS prioritarias que el propio loop() de Arduino.
 *
 * El experimento que se compila se elige con la macro EJEMPLO (ver
 * platformio.ini): un entorno por ejemplo.
 */
#include <Arduino.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ejemplos.h"
#include "plataforma.h"

#ifndef EJEMPLO
#define EJEMPLO 1
#endif

static const char *TAG = "ARRANQUE";

void setup()
{
    Serial.begin(115200);

    // Margen para que el monitor serie se enganche antes de los primeros logs.
    delay(600);

    plat_log('I', TAG, "ESP32-C6 - ejemplos de FreeRTOS sobre un unico nucleo");
    plat_log('I', TAG, "Nucleos disponibles: %d | frecuencia: %lu MHz",
             (int)ESP.getChipCores(), (unsigned long)getCpuFrequencyMhz());
    plat_log('I', TAG, "Tick de FreeRTOS: %d Hz | prioridad maxima: %d",
             (int)configTICK_RATE_HZ, (int)configMAX_PRIORITIES - 1);
    plat_log('I', TAG, "Ejecutando el ejemplo %d", EJEMPLO);

#if   EJEMPLO == 1
    ej1_arrancar();
#elif EJEMPLO == 2
    ej2_arrancar();
#elif EJEMPLO == 3
    ej3_arrancar();
#elif EJEMPLO == 4
    ej4_arrancar();
#elif EJEMPLO == 5
    ej5_arrancar();
#else
#error "Defina EJEMPLO entre 1 y 5 (vea platformio.ini)"
#endif

    plat_log('I', TAG, "Tareas creadas. El planificador toma el control.");
}

void loop()
{
    /*
     * loop() es el cuerpo de una tarea de FreeRTOS de prioridad 1. Si se
     * dejara vacio, se convertiria en un bucle cerrado que nunca cede el
     * nucleo y dejaria sin CPU a la tarea Idle: exactamente el fallo que
     * demuestra el ejemplo 3. Por eso se bloquea.
     */
    vTaskDelay(pdMS_TO_TICKS(1000));
}
