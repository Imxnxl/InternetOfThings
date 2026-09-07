/*
 * traza.h - Registro de cambios de contexto del planificador
 *
 * FreeRTOS invoca traza_registrar_switch() desde dentro de vTaskSwitchContext()
 * (macro traceTASK_SWITCHED_IN de FreeRTOSConfig.h), es decir, en CADA cambio
 * de contexto real. No es una estimacion: es el propio kernel avisando de que
 * ha guardado el estado de una tarea y ha cargado el de otra.
 *
 * Con esos datos se generan los CSV que alimentan el cronograma del informe.
 */
#ifndef TRAZA_H
#define TRAZA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void     traza_iniciar(const char *nombre_ejemplo);
void     traza_registrar_switch(const char *pcNombre, uint32_t ulTick); /* del kernel */
void     traza_led(int pin, int estado);
void     traza_evento(const char *etiqueta, uint32_t valor);
uint64_t traza_us_actuales(void);      /* reloj de pared del PC          */
uint32_t traza_ms_desde_idle(uint32_t tick_actual); /* tiempo SIMULADO   */
void     traza_actualizar_tick(uint32_t tick);      /* llamada cada tick */
void     traza_volcar(const char *directorio, const char *nombre_ejemplo);

#ifdef __cplusplus
}
#endif

#endif /* TRAZA_H */
