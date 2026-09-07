/*
 * EJEMPLO 3 - Starvation (inanicion) y Watchdog
 * =============================================
 * Este ejemplo NO esta en el material: se anade para comprobar de forma
 * experimental el riesgo que el material describe en prosa:
 *
 *   "Si una tarea de alta prioridad entra en un bucle infinito sin soltar el
 *    procesador, las tareas de menor prioridad (e incluso funciones internas
 *    como el Wi-Fi o el Watchdog Timer) nunca se ejecutaran, provocando un
 *    reinicio por software."
 *
 * Montaje del experimento:
 *
 *   vTareaGolosa   prio 3 - alterna entre portarse bien y acaparar la CPU
 *   vTareaVictima  prio 2 - deberia latir cada 100 ms; contamos cuantos
 *                           latidos pierde mientras la golosa acapara
 *   vTareaFondo    prio 1 - tarea de baja prioridad, la primera en morir
 *
 * La golosa arranca con 5 s de buen comportamiento (trabaja y se bloquea con
 * vTaskDelay). Despues entra en un bucle de calculo de 8 s SIN bloquearse.
 * Al ser la de mayor prioridad, el planificador no puede quitarsela de encima:
 * ni las tareas de prioridad 2 y 1 ni la tarea Idle vuelven a ejecutarse.
 *
 * En un ESP32-C6 real, el Task Watchdog Timer (5 s por defecto) detecta que la
 * tarea Idle lleva demasiado tiempo sin correr y reinicia el chip. En el
 * simulador se reproduce ese mismo mecanismo desde el tick hook y se REPORTA
 * el timeout sin reiniciar, para poder observar tambien la recuperacion.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "plataforma.h"
#include "ejemplos.h"

static const char *TAG = "STARVATION";

#define MS_PORTANDOSE_BIEN   5000   /* fase inicial sana                 */
#define MS_ACAPARANDO        8000   /* fase de bucle sin ceder el nucleo */
#define PERIODO_VICTIMA_MS    100   /* latido esperado de la victima     */

/* Bandera compartida: la escribe la golosa, la leen las demas solo para el
 * informe. En un solo nucleo con acceso alineado de 32 bits la lectura es
 * atomica, por eso basta con volatile y no hace falta un mutex. */
static volatile int acaparando = 0;

/* TAREA GOLOSA (prioridad 3, la mas alta de las tres) */
static void vTareaGolosa(void *pvParameters)
{
    (void)pvParameters;

    /* --- FASE A: comportamiento correcto --------------------------------- */
    LOG_I(TAG, "FASE A: la tarea de prioridad 3 trabaja y CEDE el nucleo.");

    TickType_t fin_fase_a = xTaskGetTickCount() + pdMS_TO_TICKS(MS_PORTANDOSE_BIEN);
    while (xTaskGetTickCount() < fin_fase_a) {
        LOG_I("GOLOSA", "Trabajo corto hecho, me bloqueo 250 ms.");
        vTaskDelay(pdMS_TO_TICKS(250));
    }

    /* --- FASE B: bucle de calculo sin bloqueo ---------------------------- */
    LOG_E(TAG, "FASE B: entro en bucle de calculo %d ms SIN vTaskDelay(). "
               "A partir de aqui nadie mas puede ejecutarse.", MS_ACAPARANDO);
    acaparando = 1;

    TickType_t fin_fase_b = xTaskGetTickCount() + pdMS_TO_TICKS(MS_ACAPARANDO);
    volatile double basura = 0.0;
    while (xTaskGetTickCount() < fin_fase_b) {
        /* Calculo pesado ficticio. Lo importante no es el resultado, sino que
         * la tarea nunca llama a una funcion de bloqueo. */
        for (int i = 1; i < 20000; i++) {
            basura += 1.0 / (double)i;
        }
    }

    acaparando = 0;

    /* Se consume el resultado para que el compilador no pueda eliminar el
     * bucle de carga por optimizacion. */
    LOG_I(TAG, "Bucle de calculo terminado (acumulado: %.4f).", (double)basura);

    /* --- FASE C: vuelve a portarse bien ---------------------------------- */
    LOG_I(TAG, "FASE C: suelto el nucleo. El sistema deberia recuperarse.");

    while (1) {
        LOG_I("GOLOSA", "De nuevo cediendo el nucleo correctamente.");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* TAREA VICTIMA (prioridad 2): mide cuantos latidos pierde */
static void vTareaVictima(void *pvParameters)
{
    (void)pvParameters;

    uint32_t latidos        = 0;
    uint32_t perdidos_total = 0;
    uint32_t ms_ultimo      = plat_millis();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(PERIODO_VICTIMA_MS));

        uint32_t ahora = plat_millis();
        uint32_t hueco = ahora - ms_ultimo;   /* separacion real entre latidos */
        ms_ultimo = ahora;
        latidos++;

        /* Con el nucleo libre el hueco es ~100 ms. Si la tarea golosa nos ha
         * dejado sin CPU, el hueco se dispara: esa diferencia es la medida
         * directa de la inanicion. */
        if (hueco > (PERIODO_VICTIMA_MS * 2)) {
            uint32_t perdidos = (hueco / PERIODO_VICTIMA_MS) - 1;
            perdidos_total += perdidos;
            LOG_E("VICTIMA", "Latido #%lu tras %lu ms (esperado %d ms) -> "
                             "%lu periodos perdidos (acumulado: %lu)",
                  (unsigned long)latidos,
                  (unsigned long)hueco,
                  PERIODO_VICTIMA_MS,
                  (unsigned long)perdidos,
                  (unsigned long)perdidos_total);
            plat_evento("periodos_perdidos", perdidos);
            plat_evento("hueco_victima_ms", hueco);
        } else if ((latidos % 10) == 0) {
            LOG_I("VICTIMA", "Latido #%lu puntual (hueco real %lu ms).",
                  (unsigned long)latidos, (unsigned long)hueco);
        }
    }
}

/* TAREA DE FONDO (prioridad 1): la mas debil de todas */
static void vTareaFondo(void *pvParameters)
{
    (void)pvParameters;

    uint32_t ciclos = 0;

    while (1) {
        ciclos++;
        LOG_W("FONDO", "Tarea de prioridad 1 viva. Ciclo %lu.", (unsigned long)ciclos);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void ej3_arrancar(void)
{
    LOG_I(TAG, "Experimento de inanicion en un nucleo unico.");
    LOG_I(TAG, "Prioridades -> Golosa: 3 | Victima: 2 | Fondo: 1");

    xTaskCreate(vTareaVictima, "Victima", 2048, NULL, 2, NULL);
    xTaskCreate(vTareaFondo,   "Fondo",   2048, NULL, 1, NULL);
    xTaskCreate(vTareaGolosa,  "Golosa",  4096, NULL, 3, NULL);
}
