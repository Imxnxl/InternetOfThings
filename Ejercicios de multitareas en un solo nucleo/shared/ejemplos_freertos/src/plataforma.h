/*
 * plataforma.h - Capa de abstraccion minima (HAL)
 *
 * Los ejemplos de este repositorio se compilan SIN CAMBIOS para dos destinos:
 *
 *   1) ESP32-C6 real  -> implementacion en firmware/src/plataforma_arduino.cpp
 *   2) Simulador PC   -> implementacion en simulacion/shim/plataforma_host.c
 *
 * Solo cambia la implementacion de estas cuatro funciones; la logica de las
 * tareas de FreeRTOS (prioridades, delays, colas) es literalmente el mismo
 * codigo en ambos casos. Eso es lo que permite que la evidencia obtenida en el
 * simulador sea representativa del comportamiento en la placa.
 */
#ifndef PLATAFORMA_H
#define PLATAFORMA_H

#include <stdint.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Niveles de log equivalentes a ESP_LOGI / ESP_LOGW / ESP_LOGE de ESP-IDF */
void plat_log(char nivel, const char *tag, const char *fmt, ...);

#define LOG_I(tag, ...) plat_log('I', tag, __VA_ARGS__)
#define LOG_W(tag, ...) plat_log('W', tag, __VA_ARGS__)
#define LOG_E(tag, ...) plat_log('E', tag, __VA_ARGS__)

/* LED: en la placa es un GPIO real, en el simulador es un LED virtual cuyo
 * estado se registra en un fichero de traza para poder graficarlo despues. */
void plat_led_init(int pin);
void plat_led_write(int pin, int estado);

/* Milisegundos desde el arranque (equivalente a millis() / esp_timer). */
uint32_t plat_millis(void);

/* Marca un evento con nombre en la traza (se usa para medir latencias). */
void plat_evento(const char *etiqueta, uint32_t valor);

#ifdef __cplusplus
}
#endif

#endif /* PLATAFORMA_H */
