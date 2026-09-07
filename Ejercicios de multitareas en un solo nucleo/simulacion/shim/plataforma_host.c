/*
 * plataforma_host.c - Implementacion del HAL para la simulacion en PC
 *
 * Equivalencias con el ESP32-C6:
 *
 *   plat_log()       <-> ESP_LOGI/W/E  (se replica el formato de ESP-IDF)
 *   plat_led_write() <-> digitalWrite() sobre el GPIO 8
 *   plat_millis()    <-> millis()
 *
 * Aqui viven ademas los hooks del kernel, incluida la emulacion del Task
 * Watchdog Timer de ESP-IDF.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "plataforma.h"
#include "traza.h"

/* ESP-IDF reinicia el chip si la tarea Idle no corre en este tiempo.
 * Valor por defecto de CONFIG_ESP_TASK_WDT_TIMEOUT_S. */
#define TWDT_TIMEOUT_MS   ( 5U * 1000U )

static volatile int s_twdt_disparado = 0;

/* ------------------------------------------------------------------ Log -- */
void plat_log(char nivel, const char *tag, const char *fmt, ...)
{
    /* Nombre de la tarea que esta ejecutandose ahora mismo. No forma parte del
     * formato de ESP-IDF, pero es la evidencia directa de quien tiene la CPU. */
    const char *tarea = "-";
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        TaskHandle_t h = xTaskGetCurrentTaskHandle();
        if (h != NULL) {
            tarea = pcTaskGetName(h);
        }
    }

    printf("%c (%lu) [%-13s] %s: ", nivel,
           (unsigned long)plat_millis(), tarea, tag);

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    printf("\n");
    fflush(stdout);
}

/* ------------------------------------------------------------------ LED -- */
void plat_led_init(int pin)
{
    LOG_I("GPIO", "Pin %d configurado como salida (LED virtual).", pin);
    traza_led(pin, 0);
}

void plat_led_write(int pin, int estado)
{
    traza_led(pin, estado);
}

/* --------------------------------------------------------------- Tiempo -- */
/*
 * Reloj del sistema = tick de FreeRTOS, igual que millis() en el ESP32-C6.
 *
 * IMPORTANTE para interpretar los resultados: NO se usa el reloj de pared del
 * PC. El port Win32 no consigue sostener un tick exacto de 1 kHz (Windows no
 * garantiza temporizadores de 1 ms), por lo que la simulacion avanza mas
 * despacio que el tiempo real. Lo que no cambia es el tiempo SIMULADO del
 * microcontrolador, que es el unico relevante: un vTaskDelay(500) sigue siendo
 * exactamente 500 ticks, igual que en el chip.
 */
uint32_t plat_millis(void)
{
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        return 0;
    }
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

void plat_evento(const char *etiqueta, uint32_t valor)
{
    traza_evento(etiqueta, valor);
}

/* ------------------------------------------------- Hooks del kernel ------ */

/*
 * Emulacion del Task Watchdog Timer de ESP-IDF.
 *
 * Este hook se ejecuta en el contexto del tick del sistema, es decir, con la
 * misma independencia que tiene un temporizador hardware: se dispara aunque
 * una tarea este acaparando la CPU. Ese es justo el mecanismo por el que el
 * TWDT real puede detectar la inanicion y reiniciar el chip.
 *
 * Diferencia deliberada con el hardware: aqui NO se reinicia nada, solo se
 * informa, para poder observar tambien la fase de recuperacion.
 */
void vApplicationTickHook(void)
{
    /* Este hook corre en el contexto del tick: hay que pedir el contador con
     * la variante FromISR. */
    uint32_t tick = (uint32_t)xTaskGetTickCountFromISR();
    traza_actualizar_tick(tick);

    if (s_twdt_disparado) {
        return;
    }

    uint32_t ms_sin_idle = traza_ms_desde_idle(tick);

    if (ms_sin_idle > TWDT_TIMEOUT_MS) {
        s_twdt_disparado = 1;
        unsigned long ms = (unsigned long)(tick * portTICK_PERIOD_MS);

        printf("\n");
        printf("E (%lu) TWDT: Task watchdog got triggered.\n", ms);
        printf("E (%lu) TWDT: La tarea Idle lleva %lu ms sin ejecutarse (limite %u ms).\n",
               ms, (unsigned long)ms_sin_idle, TWDT_TIMEOUT_MS);
        printf("E (%lu) TWDT: En un ESP32-C6 real el chip se REINICIARIA aqui\n", ms);
        printf("E (%lu) TWDT: (Guru Meditation Error -> rst:0xc SW_CPU_RESET).\n", ms);
        printf("E (%lu) TWDT: La simulacion continua para observar la recuperacion.\n", ms);
        printf("\n");
        fflush(stdout);

        traza_evento("twdt_timeout_ms", ms_sin_idle);
    }
}

void vApplicationMallocFailedHook(void)
{
    fprintf(stderr, "\nE: Memoria agotada en el heap de FreeRTOS.\n");
    fflush(stderr);
    exit(1);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    fprintf(stderr, "\nE: Desbordamiento de pila en la tarea %s.\n", pcTaskName);
    fflush(stderr);
    exit(1);
}

void fallo_assert(const char *fichero, int linea)
{
    fprintf(stderr, "\nE: configASSERT fallido en %s:%d\n", fichero, linea);
    fflush(stderr);
    exit(1);
}
