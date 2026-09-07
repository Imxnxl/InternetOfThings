/*
 * EJEMPLO 2 - Comunicacion entre tareas mediante una Cola (Queue)
 * ==============================================================
 * Adaptacion fiel del segundo ejemplo del material.
 *
 *   vTareaProductora  prio 2 - mide "el sensor" cada 2000 ms y encola el dato
 *   vTareaConsumidora prio 2 - duerme en xQueueReceive(portMAX_DELAY)
 *
 * La idea central del material: el consumidor NO hace polling. Se bloquea
 * indefinidamente y consume 0% de CPU hasta que hay un dato en la cola. En
 * cuanto el productor encola, el planificador lo despierta.
 *
 * Aqui se anade instrumentacion (no presente en el material) para medir la
 * latencia real entre el envio y la recepcion, que es lo que da la evidencia
 * cuantitativa para el informe.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "plataforma.h"
#include "ejemplos.h"

static const char *TAG = "SISTEMA";

/* 1. Estructura de los datos que viajan por la cola */
typedef struct {
    int      id_lectura;
    float    temperatura;
    uint32_t ms_envio;   /* marca de tiempo, para medir la latencia */
} datos_sensor_t;

/* 2. Manejador de la cola */
static QueueHandle_t cola_sensor = NULL;

/* TAREA 1: Productora - mide y envia el dato a la cola */
static void vTareaProductora(void *pvParameters)
{
    (void)pvParameters;

    int contador = 0;
    datos_sensor_t muestra;

    while (1) {
        contador++;

        /* Simulamos la lectura de un sensor */
        muestra.id_lectura  = contador;
        muestra.temperatura = 22.5f + (float)(contador % 5);
        muestra.ms_envio    = plat_millis();

        LOG_I("PRODUCTOR", "Enviando lectura #%d a la cola...", muestra.id_lectura);

        /* Timeout 0: si la cola estuviera llena no nos bloqueamos, avisamos. */
        if (xQueueSend(cola_sensor, &muestra, 0) != pdPASS) {
            LOG_E("PRODUCTOR", "Error: la cola esta llena.");
        }

        /* El productor si usa delay, porque el que marca CADA CUANTO se mide
         * es el propio sensor. */
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/* TAREA 2: Consumidora - espera los datos y los procesa */
static void vTareaConsumidora(void *pvParameters)
{
    (void)pvParameters;

    datos_sensor_t dato_recibido;

    while (1) {
        /* La clave del ejemplo: portMAX_DELAY duerme la tarea de forma
         * indefinida. Mientras la cola este vacia, esta tarea no aparece
         * siquiera en la lista de tareas listas del planificador. */
        if (xQueueReceive(cola_sensor, &dato_recibido, portMAX_DELAY) == pdPASS) {

            uint32_t latencia = plat_millis() - dato_recibido.ms_envio;

            LOG_W("CONSUMIDOR", "Dato recibido. ID: %d, Temp: %.2f C, latencia: %lu ms",
                  dato_recibido.id_lectura,
                  (double)dato_recibido.temperatura,
                  (unsigned long)latencia);

            plat_evento("latencia_cola_ms", latencia);

            /* Al cerrar el ciclo vuelve a xQueueReceive y se duerme otra vez.
             * No hace falta ningun vTaskDelay() en esta tarea. */
        }
    }
}

void ej2_arrancar(void)
{
    LOG_I(TAG, "Iniciando sistema de colas en un solo nucleo...");

    /* 3. La cola se crea ANTES de lanzar las tareas que la usan.
     * Capacidad: 5 estructuras datos_sensor_t. */
    cola_sensor = xQueueCreate(5, sizeof(datos_sensor_t));

    if (cola_sensor != NULL) {
        /* Consumidora primero: arranca, encuentra la cola vacia y se duerme. */
        xTaskCreate(vTareaConsumidora, "Consumidor", 2048, NULL, 2, NULL);
        xTaskCreate(vTareaProductora,  "Productor",  2048, NULL, 2, NULL);
    } else {
        LOG_E(TAG, "Error al crear la cola.");
    }
}
