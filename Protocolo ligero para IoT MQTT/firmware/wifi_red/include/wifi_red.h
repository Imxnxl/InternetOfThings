/*
 * wifi_red.h - Wi-Fi con la red guardada en la placa, cambiable por USB
 * ============================================================================
 *
 * PARA QUE SIRVE
 *   Con example_connect() la red Wi-Fi queda fija al compilar (menuconfig):
 *   para usar otra, por ejemplo el punto de acceso de un celular en clase,
 *   habia que recompilar y volver a grabar. Este componente guarda la red en
 *   la memoria NVS de la placa y permite cambiarla desde tablero.html, por el
 *   cable USB, sin recompilar.
 *
 *   La red de menuconfig sigue sirviendo: se usa mientras no haya otra
 *   guardada, y vuelve a usarse con el comando WIFI_BORRAR.
 *
 * COMANDOS POR LA CONSOLA (115200 baudios, una linea cada uno)
 *   WIFI?                         muestra la red en uso (nunca la contrasena)
 *   WIFI_SET<TAB>ssid<TAB>clave   guarda esa red y reinicia la placa
 *   WIFI_BORRAR                   olvida la red guardada y reinicia
 *
 *   Las respuestas empiezan por WIFI_ESTADO, WIFI_OK o WIFI_ERROR, para que
 *   el tablero las distinga del resto del registro.
 */
#pragma once

#include "esp_err.h"

/*
 * Arranca la tarea que atiende los comandos por la consola y conecta el
 * Wi-Fi (estacion). Requiere nvs_flash_init(), esp_netif_init() y
 * esp_event_loop_create_default() hechos antes. Devuelve ESP_OK cuando la
 * placa tiene direccion IP, o ESP_FAIL si no pudo conectar; en ese caso los
 * comandos siguen atendiendose, para poder corregir la red.
 */
esp_err_t wifi_red_conectar(void);
