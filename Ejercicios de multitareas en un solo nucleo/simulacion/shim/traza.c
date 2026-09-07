#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>

#include "FreeRTOS.h"
#include "task.h"
#include "traza.h"

#define MAX_SWITCHES   600000
#define MAX_LED        200000
#define MAX_EVENTOS     20000

typedef struct { uint64_t us; uint32_t tick; const char *nombre; } switch_t;
typedef struct { uint32_t tick; int pin; int estado; } led_t;
typedef struct { uint32_t tick; char etiqueta[40]; uint32_t valor; } evento_t;

static switch_t  s_switches[MAX_SWITCHES];
static led_t     s_leds[MAX_LED];
static evento_t  s_eventos[MAX_EVENTOS];

static volatile long s_n_switch = 0;
static volatile long s_n_led    = 0;
static volatile long s_n_evento = 0;

static LARGE_INTEGER s_frecuencia;
static LARGE_INTEGER s_inicio;
static volatile uint32_t s_tick_idle = 0;  /* ultimo tick en que corrio la Idle  */
static volatile uint32_t s_tick_actual = 0;/* ultimo tick visto por la traza     */

uint64_t traza_us_actuales(void)
{
    LARGE_INTEGER ahora;
    QueryPerformanceCounter(&ahora);
    return (uint64_t)(((ahora.QuadPart - s_inicio.QuadPart) * 1000000ULL) /
                      (uint64_t)s_frecuencia.QuadPart);
}

void traza_iniciar(const char *nombre_ejemplo)
{
    (void)nombre_ejemplo;
    QueryPerformanceFrequency(&s_frecuencia);
    QueryPerformanceCounter(&s_inicio);
    s_n_switch    = 0;
    s_n_led       = 0;
    s_n_evento    = 0;
    s_tick_idle   = 0;
    s_tick_actual = 0;
}

/*
 * Llamada por el kernel en CADA cambio de contexto (macro traceTASK_SWITCHED_IN).
 * Debe ser muy corta y no puede bloquearse: se ejecuta dentro de una seccion
 * critica del planificador.
 *
 * Se guardan dos relojes distintos y no intercambiables:
 *   - tick : tiempo SIMULADO del microcontrolador (el tick de FreeRTOS). Es el
 *            que hay que usar para cualquier medida del experimento, porque es
 *            el que veria el ESP32-C6.
 *   - us   : reloj de pared del PC. Solo sirve para ordenar eventos dentro de
 *            un mismo tick y para medir cuanto dura de verdad cada rodaja.
 */
void traza_registrar_switch(const char *pcNombre, uint32_t ulTick)
{
    s_tick_actual = ulTick;

    /* La tarea Idle solo corre cuando ninguna otra tiene trabajo. Anotar
     * cuando corrio por ultima vez es exactamente el criterio que usa el
     * Task Watchdog Timer de ESP-IDF para decidir si hay inanicion. */
    if (pcNombre != NULL && strncmp(pcNombre, "IDLE", 4) == 0) {
        s_tick_idle = ulTick;
    }

    long i = s_n_switch;
    if (i < MAX_SWITCHES) {
        s_switches[i].us     = traza_us_actuales();
        s_switches[i].tick   = ulTick;
        s_switches[i].nombre = pcNombre;
        s_n_switch = i + 1;
    }
}

/*
 * El tick hook la llama en cada tick. Sin esto, el sello de tiempo de los
 * eventos se congelaria justo cuando mas interesa: durante una inanicion no
 * hay cambios de contexto, y el ultimo tick conocido se quedaria obsoleto.
 */
void traza_actualizar_tick(uint32_t tick)
{
    s_tick_actual = tick;
}

uint32_t traza_ms_desde_idle(uint32_t tick_actual)
{
    /* Con configTICK_RATE_HZ = 1000, 1 tick = 1 ms simulado. */
    return (uint32_t)((tick_actual - s_tick_idle) * portTICK_PERIOD_MS);
}

void traza_led(int pin, int estado)
{
    long i = s_n_led;
    if (i < MAX_LED) {
        s_leds[i].tick   = s_tick_actual;
        s_leds[i].pin    = pin;
        s_leds[i].estado = estado;
        s_n_led = i + 1;
    }
}

void traza_evento(const char *etiqueta, uint32_t valor)
{
    long i = s_n_evento;
    if (i < MAX_EVENTOS) {
        s_eventos[i].tick = s_tick_actual;
        strncpy(s_eventos[i].etiqueta, etiqueta, sizeof(s_eventos[i].etiqueta) - 1);
        s_eventos[i].etiqueta[sizeof(s_eventos[i].etiqueta) - 1] = '\0';
        s_eventos[i].valor = valor;
        s_n_evento = i + 1;
    }
}

static FILE *abrir(const char *dir, const char *ej, const char *sufijo)
{
    char ruta[512];
    snprintf(ruta, sizeof(ruta), "%s/%s_%s.csv", dir, ej, sufijo);
    FILE *f = fopen(ruta, "w");
    if (f == NULL) {
        fprintf(stderr, "[traza] no se pudo abrir %s\n", ruta);
    }
    return f;
}

void traza_volcar(const char *directorio, const char *nombre_ejemplo)
{
    long n;

    /* --- Cambios de contexto: tarea, inicio y fin de cada rodaja de CPU --- */
    FILE *f = abrir(directorio, nombre_ejemplo, "contexto");
    if (f) {
        fprintf(f, "tarea,tick_ms,us_inicio,us_fin,duracion_us\n");
        n = s_n_switch;
        for (long i = 0; i < n; i++) {
            uint64_t fin = (i + 1 < n) ? s_switches[i + 1].us : s_switches[i].us;
            fprintf(f, "%s,%lu,%llu,%llu,%llu\n",
                    s_switches[i].nombre ? s_switches[i].nombre : "?",
                    (unsigned long)s_switches[i].tick,
                    (unsigned long long)s_switches[i].us,
                    (unsigned long long)fin,
                    (unsigned long long)(fin - s_switches[i].us));
        }
        fclose(f);
    }

    /* --- Estado del LED virtual ------------------------------------------ */
    f = abrir(directorio, nombre_ejemplo, "led");
    if (f) {
        fprintf(f, "tick_ms,pin,estado\n");
        n = s_n_led;
        for (long i = 0; i < n; i++) {
            fprintf(f, "%lu,%d,%d\n",
                    (unsigned long)s_leds[i].tick, s_leds[i].pin, s_leds[i].estado);
        }
        fclose(f);
    }

    /* --- Eventos instrumentados (latencias, periodos perdidos) ----------- */
    f = abrir(directorio, nombre_ejemplo, "eventos");
    if (f) {
        fprintf(f, "tick_ms,etiqueta,valor\n");
        n = s_n_evento;
        for (long i = 0; i < n; i++) {
            fprintf(f, "%lu,%s,%lu\n",
                    (unsigned long)s_eventos[i].tick,
                    s_eventos[i].etiqueta,
                    (unsigned long)s_eventos[i].valor);
        }
        fclose(f);
    }

    printf("\n[traza] %ld cambios de contexto, %ld transiciones de LED, %ld eventos.\n",
           s_n_switch, s_n_led, s_n_evento);
}
