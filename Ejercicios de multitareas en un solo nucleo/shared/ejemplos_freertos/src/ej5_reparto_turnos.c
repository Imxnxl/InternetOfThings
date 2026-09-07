/*
 * EJEMPLO 5 - Time-slicing puro (reparto por turnos)
 * ==================================================
 * El material afirma:
 *
 *   "Reparto de tiempo (Time-slicing): Si creas dos o mas tareas con la misma
 *    prioridad, el nucleo reparte pequenos fragmentos de tiempo (milisegundos)
 *    a cada una mediante turnos rotativos."
 *
 * El primer ejemplo del material NO llega a demostrar esto, porque sus dos
 * tareas se bloquean con vTaskDelay() y por tanto se turnan por bloqueo, no
 * por reparto de tiempo. Para verificar la afirmacion hace falta que las dos
 * tareas compitan de verdad: misma prioridad y NINGUNA llamada de bloqueo.
 *
 *   vTareaTrabajoA  prio 2 - bucle de calculo permanente
 *   vTareaTrabajoB  prio 2 - bucle de calculo permanente, identico
 *
 * Resultado esperado si el time-slicing existe: ambas avanzan y acumulan un
 * numero de vueltas practicamente igual (reparto ~50/50), y la traza de
 * cambios de contexto muestra la alternancia en cada tick del sistema.
 *
 * Si el time-slicing NO existiera, la primera tarea en arrancar se quedaria
 * con el nucleo para siempre y la segunda marcaria 0 vueltas.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "plataforma.h"
#include "ejemplos.h"

static const char *TAG = "TURNOS";

#define MS_ENTRE_INFORMES  1000

static volatile uint32_t vueltas_a = 0;
static volatile uint32_t vueltas_b = 0;

/* Nucleo de trabajo comun a las dos tareas. Deliberadamente NO contiene
 * vTaskDelay(), ni colas, ni semaforos: solo calculo. */
static void bucle_de_trabajo(const char *nombre, volatile uint32_t *contador)
{
    volatile double acumulador = 0.0;
    TickType_t proximo_informe = xTaskGetTickCount() + pdMS_TO_TICKS(MS_ENTRE_INFORMES);

    while (1) {
        for (int i = 1; i < 5000; i++) {
            acumulador += 1.0 / (double)i;
        }
        (*contador)++;

        /* Uso real del acumulador: evita que el optimizador borre el bucle. */
        if (acumulador > 1.0e12) {
            acumulador = 0.0;
        }

        /* El informe se emite comprobando el reloj, sin bloquearse: si nos
         * bloquearamos, dejariamos de competir y el experimento perderia
         * sentido. */
        if (xTaskGetTickCount() >= proximo_informe) {
            proximo_informe += pdMS_TO_TICKS(MS_ENTRE_INFORMES);

            uint32_t va = vueltas_a;
            uint32_t vb = vueltas_b;
            uint32_t total = va + vb;
            uint32_t pct = (total > 0) ? ((va * 100) / total) : 0;

            LOG_I(nombre, "Vueltas -> A: %lu | B: %lu | reparto A/B: %lu%% / %lu%%",
                  (unsigned long)va, (unsigned long)vb,
                  (unsigned long)pct, (unsigned long)(100 - pct));
        }
    }
}

static void vTareaTrabajoA(void *pvParameters)
{
    (void)pvParameters;
    bucle_de_trabajo("TRABAJO_A", &vueltas_a);
}

static void vTareaTrabajoB(void *pvParameters)
{
    (void)pvParameters;
    bucle_de_trabajo("TRABAJO_B", &vueltas_b);
}

void ej5_arrancar(void)
{
    LOG_I(TAG, "Dos tareas de IGUAL prioridad (2) compiten sin bloquearse nunca.");
    LOG_I(TAG, "Si el reparto por turnos funciona, las dos deben avanzar al 50%%.");

    xTaskCreate(vTareaTrabajoA, "TrabajoA", 4096, NULL, 2, NULL);
    xTaskCreate(vTareaTrabajoB, "TrabajoB", 4096, NULL, 2, NULL);
}
