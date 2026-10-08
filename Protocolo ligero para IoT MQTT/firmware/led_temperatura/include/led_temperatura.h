/*
 * led_temperatura.h - LED RGB que muestra la temperatura recibida por MQTT
 * ============================================================================
 *
 * PARA QUE SIRVE
 *   El LED RGB ya no indica el estado de la conexion: muestra con un color la
 *   temperatura que llega al topico del suscriptor, es decir, la que tiene el
 *   canal despues de pasar por el broker.
 *
 *       30 C o menos            -> azul
 *       mas de 30 y menos de 50 -> verde   (por ejemplo, 40 C)
 *       50 C o mas              -> rojo
 *
 *   Hasta que llega la primera temperatura, el LED esta apagado.
 *
 * CONEXIONES (modulo LED RGB de catodo comun)
 *   R -> GPIO19   G -> GPIO20   B -> GPIO21   comun -> GND
 *
 * DE DONDE SALE LA TEMPERATURA
 *   Del campo field1 de los mensajes MQTT del canal. Se aceptan los dos
 *   formatos que circulan:
 *       {"channel_id":...,"field1":"40",...}   el JSON que reenvia ThingSpeak
 *                                              (y que envia tablero.html)
 *       field1=40&status=MQTTPublish            lo que publica el publicador
 *
 * Lo usan el publicador y el suscriptor.
 */
#pragma once

#include <stdbool.h>

/* Configura los tres pines del LED y lo deja apagado. */
void led_temperatura_iniciar(void);

/*
 * Busca la temperatura (field1) en un mensaje MQTT y pone el LED del color que
 * le corresponde. 'datos' no necesita terminar en '\0' (event->data no lo
 * hace). Devuelve true si el mensaje traia una temperatura valida.
 */
bool led_temperatura_procesar_mensaje(const char *datos, int largo);

/* Pone el LED del color que corresponde a 'temperatura_c' (grados C). */
void led_temperatura_mostrar(float temperatura_c);
