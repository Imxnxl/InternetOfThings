/*
 * main_host.c - Arranque de la simulacion en PC
 *
 * Uso:  simulador.exe <numero_ejemplo> [segundos] [directorio_salida]
 *
 * Ejecuta uno de los ejemplos del repositorio sobre el kernel real de FreeRTOS
 * (port MSVC-MingW, un solo nucleo), durante el tiempo indicado, y vuelca las
 * trazas en CSV para poder graficarlas.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "plataforma.h"
#include "ejemplos.h"
#include "traza.h"

/* Prioridad del supervisor: por encima de cualquier tarea de los ejemplos
 * (la mas alta que usan es 5) pero por debajo de la tarea de timers. Pasa
 * casi todo el tiempo bloqueado, asi que no altera el experimento. */
#define PRIORIDAD_SUPERVISOR   ( configMAX_PRIORITIES - 2 )

static int         s_ejemplo   = 1;
static int         s_segundos  = 15;
static const char *s_salida    = "salidas";
static char        s_nombre[32];

/* El supervisor deja correr el experimento el tiempo pedido y despues cierra
 * la simulacion volcando las trazas. En un microcontrolador esto no existe:
 * el firmware corre indefinidamente. */
static void vTareaSupervisor(void *pvParameters)
{
    (void)pvParameters;

    vTaskDelay(pdMS_TO_TICKS(s_segundos * 1000));

    printf("\n=== Fin de la simulacion (%d s) =========================\n", s_segundos);
    traza_volcar(s_salida, s_nombre);
    fflush(stdout);

    exit(0);
}

int main(int argc, char **argv)
{
    if (argc > 1) { s_ejemplo  = atoi(argv[1]); }
    if (argc > 2) { s_segundos = atoi(argv[2]); }
    if (argc > 3) { s_salida   = argv[3];       }

    if (s_ejemplo < 1 || s_ejemplo > 5) {
        fprintf(stderr, "Ejemplo no valido. Use 1..5.\n");
        return 1;
    }

    snprintf(s_nombre, sizeof(s_nombre), "ej%d", s_ejemplo);

    /* Salida sin buffer de linea para que el log se vea en orden real */
    setvbuf(stdout, NULL, _IOLBF, 4096);

    traza_iniciar(s_nombre);

    printf("========================================================\n");
    printf(" Simulacion FreeRTOS - ESP32-C6 (nucleo unico)\n");
    printf(" Kernel: FreeRTOS %s, port MSVC-MingW (1 tarea a la vez)\n", tskKERNEL_VERSION_NUMBER);
    printf(" Ejemplo: %d   Duracion: %d s   Tick: %d Hz\n",
           s_ejemplo, s_segundos, (int)configTICK_RATE_HZ);
    printf(" Preemcion: %s   Time-slicing: %s\n",
           configUSE_PREEMPTION   ? "SI" : "NO",
           configUSE_TIME_SLICING ? "SI" : "NO");
    printf("========================================================\n\n");

    switch (s_ejemplo) {
        case 1: ej1_arrancar(); break;
        case 2: ej2_arrancar(); break;
        case 3: ej3_arrancar(); break;
        case 4: ej4_arrancar(); break;
        case 5: ej5_arrancar(); break;
    }

    xTaskCreate(vTareaSupervisor, "Supervisor", 4096, NULL, PRIORIDAD_SUPERVISOR, NULL);

    /* A partir de aqui manda el planificador y esta llamada no retorna. */
    vTaskStartScheduler();

    fprintf(stderr, "El planificador no pudo arrancar (heap insuficiente).\n");
    return 1;
}
