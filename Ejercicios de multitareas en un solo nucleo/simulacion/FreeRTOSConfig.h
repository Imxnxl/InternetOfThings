/*
 * FreeRTOSConfig.h - Configuracion del kernel para la simulacion en PC
 *
 * Se usa el port oficial MSVC-MingW de FreeRTOS, que emula un microcontrolador
 * de UN SOLO NUCLEO: cada tarea es un hilo de Windows, pero el port garantiza
 * que solo UNO este ejecutandose en cada instante, exactamente igual que en el
 * ESP32-C6. El planificador (tasks.c, queue.c) es el codigo real de FreeRTOS,
 * no una imitacion.
 *
 * Los parametros relevantes se han igualado a los de ESP-IDF para el ESP32-C6:
 *   - configTICK_RATE_HZ 1000  -> el tick por defecto de ESP-IDF es 100 Hz,
 *     pero se sube a 1000 Hz (opcion CONFIG_FREERTOS_HZ=1000, muy habitual)
 *     para poder medir las latencias con resolucion de 1 ms.
 *   - configUSE_PREEMPTION y configUSE_TIME_SLICING activos, como en ESP-IDF.
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>

/* --- Planificacion ------------------------------------------------------- */
#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  1   /* turnos entre iguales   */
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0   /* obligatorio en MinGW   */
#define configMAX_PRIORITIES                    ( 10 )
#define configTICK_RATE_HZ                      ( ( TickType_t ) 1000 )
#define configMINIMAL_STACK_SIZE                ( ( unsigned short ) 70 )
#define configMAX_TASK_NAME_LEN                 ( 16 )
#define configUSE_16_BIT_TICKS                  0

/* --- Memoria ------------------------------------------------------------- */
#define configSUPPORT_DYNAMIC_ALLOCATION         1
#define configSUPPORT_STATIC_ALLOCATION          0
#define configTOTAL_HEAP_SIZE                    ( ( size_t ) ( 4 * 1024 * 1024 ) )

/* --- Objetos de sincronizacion ------------------------------------------- */
#define configUSE_MUTEXES                        1
#define configUSE_RECURSIVE_MUTEXES              1
#define configUSE_COUNTING_SEMAPHORES            1
#define configQUEUE_REGISTRY_SIZE                10

/* --- Software timers ----------------------------------------------------- */
#define configUSE_TIMERS                         1
#define configTIMER_TASK_PRIORITY                ( configMAX_PRIORITIES - 1 )
#define configTIMER_QUEUE_LENGTH                 10
#define configTIMER_TASK_STACK_DEPTH             ( configMINIMAL_STACK_SIZE * 2 )

/* --- Hooks --------------------------------------------------------------- */
#define configUSE_IDLE_HOOK                      0
#define configUSE_TICK_HOOK                      1   /* emula el Task WDT     */
#define configUSE_MALLOC_FAILED_HOOK             1
#define configCHECK_FOR_STACK_OVERFLOW           0   /* no aplica en el port Win32 */
#define configUSE_DAEMON_TASK_STARTUP_HOOK       0

/* --- Estadisticas y trazas ----------------------------------------------- */
#define configUSE_TRACE_FACILITY                 1
#define configUSE_STATS_FORMATTING_FUNCTIONS     1
#define configGENERATE_RUN_TIME_STATS            0

/* Enganche de traza: FreeRTOS invoca esta macro DENTRO de vTaskSwitchContext,
 * es decir, en cada cambio de contexto real del planificador. Es la fuente de
 * datos con la que se construye el cronograma de ejecucion del informe. */
extern void traza_registrar_switch( const char *pcNombre, uint32_t ulTick );
#define traceTASK_SWITCHED_IN() \
    traza_registrar_switch( pxCurrentTCB->pcTaskName, ( uint32_t ) xTickCount )

/* --- API incluida en la compilacion -------------------------------------- */
#define INCLUDE_vTaskPrioritySet                 1
#define INCLUDE_uxTaskPriorityGet                1
#define INCLUDE_vTaskDelete                      1
#define INCLUDE_vTaskSuspend                     1
#define INCLUDE_vTaskDelayUntil                  1
#define INCLUDE_vTaskDelay                       1
#define INCLUDE_xTaskGetSchedulerState           1
#define INCLUDE_xTaskGetCurrentTaskHandle        1
#define INCLUDE_uxTaskGetStackHighWaterMark      1
#define INCLUDE_xTaskGetIdleTaskHandle           1
#define INCLUDE_eTaskGetState                    1
#define INCLUDE_xTaskAbortDelay                  1
#define INCLUDE_xTaskGetHandle                   1

/* --- Asercion ------------------------------------------------------------ */
extern void fallo_assert( const char *fichero, int linea );
#define configASSERT( x )   if( ( x ) == 0 ) { fallo_assert( __FILE__, __LINE__ ); }

#endif /* FREERTOS_CONFIG_H */
