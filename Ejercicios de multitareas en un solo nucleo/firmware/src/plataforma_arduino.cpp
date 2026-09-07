/*
 * plataforma_arduino.cpp - Implementacion del HAL para el ESP32-C6 real
 *
 * Es la contraparte de simulacion/shim/plataforma_host.c. Los ejemplos de
 * shared/ejemplos_freertos no cambian ni una linea: solo cambia lo que hay
 * detras de estas cuatro funciones.
 */
#include <Arduino.h>
#include <stdarg.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "plataforma.h"

/* ------------------------------------------------------------------ Log -- */
void plat_log(char nivel, const char *tag, const char *fmt, ...)
{
    /* Mismo formato que en el simulador, para poder comparar las dos salidas
     * linea a linea: nivel, milisegundos, tarea que tiene la CPU y etiqueta. */
    const char *tarea = pcTaskGetName(NULL);

    Serial.printf("%c (%lu) [%-13s] %s: ",
                  nivel, (unsigned long)plat_millis(),
                  tarea ? tarea : "-", tag);

    char linea[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(linea, sizeof(linea), fmt, args);
    va_end(args);

    Serial.println(linea);
}

/* ------------------------------------------------------------------ LED -- */
void plat_led_init(int pin)
{
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    plat_log('I', "GPIO", "Pin %d configurado como salida.", pin);
}

void plat_led_write(int pin, int estado)
{
    digitalWrite(pin, estado ? HIGH : LOW);
}

/* --------------------------------------------------------------- Tiempo -- */
uint32_t plat_millis(void)
{
    return (uint32_t)millis();
}

/*
 * En el simulador esta funcion guarda el valor en una traza CSV para poder
 * graficarlo despues. En la placa no hay disco donde escribir, asi que
 * simplemente se emite por el puerto serie con un prefijo reconocible: si se
 * captura la sesion del monitor a un fichero, las lineas MEDIDA se pueden
 * extraer con un grep y procesar igual que los CSV de la simulacion.
 */
void plat_evento(const char *etiqueta, uint32_t valor)
{
    Serial.printf("MEDIDA,%lu,%s,%lu\n",
                  (unsigned long)plat_millis(), etiqueta,
                  (unsigned long)valor);
}
