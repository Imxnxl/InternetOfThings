/*
 * EJEMPLO 4 - Preemcion por prioridad
 * ===================================
 * Ejemplo anadido para comprobar la primera regla que enuncia el material:
 *
 *   "Por Prioridades: FreeRTOS siempre ejecuta la tarea lista que tenga la
 *    prioridad mas alta. Si una tarea importante se activa, pausa la actual
 *    de inmediato."
 *
 * Montaje:
 *
 *   vTareaCalculo   prio 1 - trabaja continuamente (nunca se bloquea salvo
 *                            un yield puntual). Representa el "trabajo util"
 *                            de fondo que consume todo el tiempo sobrante.
 *   vTareaUrgente   prio 5 - duerme 2 s y, al despertar, debe expulsar a la
 *                            de calculo INMEDIATAMENTE. Mide la latencia de
 *                            respuesta, que es la cifra que interesa en un
 *                            sistema de tiempo real.
 *
 * Lo que se mide: cuanto tarda la tarea urgente en obtener la CPU desde el
 * instante teorico en el que debia despertar. Si la preemcion funciona, esa
 * latencia debe ser de 1 tick o menos, aunque la CPU estuviera al 100%.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "plataforma.h"
#include "ejemplos.h"

static const char *TAG = "PRIORIDAD";

#define PERIODO_URGENTE_MS  2000

static volatile uint32_t vueltas_calculo = 0;

/* TAREA DE CALCULO (prioridad 1): consume todo el tiempo libre */
static void vTareaCalculo(void *pvParameters)
{
    (void)pvParameters;

    volatile double acumulador = 0.0;

    while (1) {
        for (int i = 1; i < 50000; i++) {
            acumulador += 1.0 / (double)i;
        }
        vueltas_calculo++;

        /* Se acota el acumulador para que no desborde y, de paso, se le da uso
         * real: asi el compilador no puede descartar el bucle de carga. */
        if (acumulador > 1.0e12) {
            acumulador = 0.0;
        }

        /* taskYIELD() solo cede el turno a tareas de IGUAL prioridad; no es
         * necesario para que la tarea urgente entre (esa entra por prioridad),
         * pero deja el ejemplo mas limpio y evita monopolizar la tarea Idle. */
        taskYIELD();
    }
}

/* TAREA URGENTE (prioridad 5): simula la atencion a un evento critico */
static void vTareaUrgente(void *pvParameters)
{
    (void)pvParameters;

    TickType_t proximo_despertar = xTaskGetTickCount();
    uint32_t   activaciones = 0;

    while (1) {
        /* vTaskDelayUntil deja en proximo_despertar el instante TEORICO en el
         * que la tarea debia volver a estar lista. Comparandolo con el tick
         * real de reanudacion obtenemos la latencia de preemcion. */
        vTaskDelayUntil(&proximo_despertar, pdMS_TO_TICKS(PERIODO_URGENTE_MS));

        TickType_t ahora    = xTaskGetTickCount();
        TickType_t latencia = ahora - proximo_despertar;   /* en ticks */
        activaciones++;

        uint32_t vueltas = vueltas_calculo;
        vueltas_calculo = 0;

        LOG_W(TAG, "Evento critico #%lu atendido. Latencia: %lu ticks (%lu ms). "
                   "La tarea de fondo habia dado %lu vueltas.",
              (unsigned long)activaciones,
              (unsigned long)latencia,
              (unsigned long)(latencia * portTICK_PERIOD_MS),
              (unsigned long)vueltas);

        plat_evento("latencia_preemcion_ticks", (uint32_t)latencia);
    }
}

void ej4_arrancar(void)
{
    LOG_I(TAG, "Comprobando la preemcion por prioridad.");
    LOG_I(TAG, "Calculo(prio 1) satura la CPU; Urgente(prio 5) debe expulsarla.");

    xTaskCreate(vTareaCalculo, "Calculo", 4096, NULL, 1, NULL);
    xTaskCreate(vTareaUrgente, "Urgente", 4096, NULL, 5, NULL);
}
