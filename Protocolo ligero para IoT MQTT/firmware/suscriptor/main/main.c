/*
 * main.c - Suscriptor MQTT: recibe al instante la temperatura del canal
 * ============================================================================
 * Practica: Protocolo ligero para IoT MQTT - Suscriptor (ESP-IDF v6.1)
 * ============================================================================
 *
 * QUE HACE
 *   1. Conecta la placa al Wi-Fi y al broker MQTT de ThingSpeak, igual que el
 *      publicador y con las mismas credenciales.
 *   2. Al conectar, se suscribe al topico del canal:
 *
 *          channels/<CHANNEL_ID>/subscribe
 *
 *   3. Cada vez que el canal recibe un dato (desde el publicador, desde otro
 *      dispositivo o desde el navegador), el broker se lo reenvia a la placa
 *      y salta el evento MQTT_EVENT_DATA, sin que la placa pregunte nada
 *      (sin polling). Se imprimen el topico y el mensaje.
 *   4. Accion: field1 es la temperatura, y el LED RGB toma su color
 *      (componente firmware/led_temperatura):
 *          30 C o menos azul, entre 30 y 50 C verde, 50 C o mas rojo.
 *
 * CON UN BROKER PUBLICO (credenciales.broker_publico.h)
 *   Nadie reenvia los datos de .../publish a .../subscribe, asi que tambien se
 *   escucha channels/<ID>/publish: el LED muestra la temperatura del
 *   publicador (si otra placa lo ejecuta) y la que envie el tablero.
 *
 * CAMBIOS RESPECTO AL CODIGO DEL PDF
 *   - URI del broker: mqtt://mqtt3.thingspeak.com (igual que en el publicador).
 *   - MQTT_EVENT_SUBCRIBED -> MQTT_EVENT_SUBSCRIBED. Con la errata del PDF el
 *     programa no compila.
 *   - ThingSpeak no reenvia el texto "field1=..." que se publico, sino la
 *     entrada completa del canal en formato JSON ("field1":"40", ...). Por eso
 *     la comparacion strncmp(event->data, "field1=1", 8) del PDF nunca se
 *     cumple. Aqui se busca el valor de "field1" dentro del mensaje.
 *   - Se comprueba la respuesta del broker a la suscripcion: si el dispositivo
 *     MQTT no tiene permiso de suscripcion en el canal, el broker la rechaza.
 *
 * CONEXIONES (modulo LED RGB de catodo comun)
 *   R -> GPIO19   G -> GPIO20   B -> GPIO21   comun -> GND
 *   Sin LED, el color se ve igualmente en el monitor.
 */
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "wifi_red.h"                   // Wi-Fi: red guardada en NVS, cambiable por USB
#include "led_temperatura.h"            // LED RGB: color segun la temperatura recibida

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "credenciales.h"               // TS_CHANNEL_ID, TS_CLIENT_ID, TS_USERNAME, TS_PASSWORD

static const char *TAG = "SUSCRIPTOR";

/* ------------------------------------------------------ Configuracion -- */

// Broker MQTT de ThingSpeak. "mqtt://" usa el puerto 1883, sin cifrar.
// credenciales.h puede definir otro (ver credenciales.broker_publico.h).
#ifndef TS_BROKER_URI
#define TS_BROKER_URI "mqtt://mqtt3.thingspeak.com"
#endif

// Ahorro de energia del Wi-Fi. Con 0 se deja el de ESP-IDF (WIFI_PS_MIN_MODEM):
// la radio duerme entre las balizas del router, que guarda los mensajes que
// llegan para la placa hasta la siguiente baliza. Gasta menos, pero cada
// mensaje tarda mas en llegar (seccion 5.4 del informe). Con 1 la radio queda
// siempre encendida.
#define WIFI_SIEMPRE_ENCENDIDO  0

static esp_mqtt_client_handle_t cliente_mqtt;

/* -------------------------------------------------------------- MQTT -- */

static void mqtt_event_handler(void *argumentos, esp_event_base_t base,
                               int32_t id_del_evento, void *datos_del_evento)
{
    esp_mqtt_event_handle_t evento = datos_del_evento;
    char topico_de_suscripcion[128];

    switch ((esp_mqtt_event_id_t)id_del_evento) {
        case MQTT_EVENT_CONNECTED: {
            ESP_LOGI(TAG, "Conectado al Broker. Suscribiendose al canal...");

            // Construimos el topico de suscripcion para ThingSpeak
            snprintf(topico_de_suscripcion, sizeof(topico_de_suscripcion),
                     "channels/%s/subscribe", TS_CHANNEL_ID);

            // Nos suscribimos con QoS 0. Se hace en cada conexion: si el broker
            // corta y el cliente reconecta, la suscripcion se renueva sola.
            int id_del_mensaje = esp_mqtt_client_subscribe(cliente_mqtt, topico_de_suscripcion, 0);
            ESP_LOGI(TAG, "Suscripcion a %s enviada con exito, ID de mensaje: %d",
                     topico_de_suscripcion, id_del_mensaje);

            // Sin ThingSpeak nadie reenvia lo publicado: se escucha tambien .../publish
            bool broker_es_thingspeak = (strstr(TS_BROKER_URI, "thingspeak") != NULL);
            if (!broker_es_thingspeak) {
                snprintf(topico_de_suscripcion, sizeof(topico_de_suscripcion),
                         "channels/%s/publish", TS_CHANNEL_ID);
                esp_mqtt_client_subscribe(cliente_mqtt, topico_de_suscripcion, 0);
                ESP_LOGI(TAG, "Suscripcion a %s enviada (broker sin ThingSpeak)", topico_de_suscripcion);
            }
            break;
        }

        case MQTT_EVENT_SUBSCRIBED:
            // El broker responde con un codigo por topico (0x00 = aceptada con
            // QoS 0; 0x80 = rechazada). El cliente lo traduce a error_type.
            if (evento->error_handle->error_type == MQTT_ERROR_TYPE_SUBSCRIBE_FAILED) {
                ESP_LOGE(TAG, "El broker rechazo la suscripcion. En ThingSpeak, el "
                              "dispositivo MQTT necesita 'Allow Subscribe' en este canal");
            } else {
                ESP_LOGI(TAG, "Suscripcion confirmada por el Broker. Esperando datos...");
            }
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "NOTIFICACION RECIBIDA!");

            // Imprimir el topico de donde provienen los datos
            printf("Topico: %.*s\r\n", evento->topic_len, evento->topic);

            // Imprimir el contenido del mensaje (Payload)
            printf("Datos recibidos: %.*s\r\n", evento->data_len, evento->data);

            // Leer la temperatura (field1) y poner el LED del color que le toca
            led_temperatura_procesar_mensaje(evento->data, evento->data_len);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Desconectado del Broker.");
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "Error en el entorno MQTT");
            if (evento->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                ESP_LOGE(TAG, "El broker rechazo la conexion (codigo %d). Revise TS_CLIENT_ID, "
                              "TS_USERNAME y TS_PASSWORD en credenciales.h",
                         evento->error_handle->connect_return_code);
            }
            break;

        default:
            break;
    }
}

/* ----------------------------------------------------------- app_main -- */

void app_main(void)
{
    ESP_LOGI(TAG, "Iniciando ESP32-C6 en modo Suscriptor...");
    led_temperatura_iniciar();           // apagado hasta recibir la primera temperatura

    if (strncmp(TS_CHANNEL_ID, "TU_", 3) == 0 || strncmp(TS_CLIENT_ID, "TU_", 3) == 0) {
        ESP_LOGW(TAG, "credenciales.h aun tiene los valores de la plantilla: "
                      "el broker rechazara la conexion");
    }

    // Inicializacion del almacenamiento NVS
    esp_err_t resultado = nvs_flash_init();
    if (resultado == ESP_ERR_NVS_NO_FREE_PAGES || resultado == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        resultado = nvs_flash_init();
    }
    ESP_ERROR_CHECK(resultado);

    // Inicializar la pila de red nativa
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Conexion Wi-Fi: red guardada en la placa (tablero.html, por USB) o, si no
    // hay ninguna, la de idf.py menuconfig. Ver firmware/wifi_red.
    if (wifi_red_conectar() != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo conectar al Wi-Fi (solo redes de 2,4 GHz). Cambie la red "
                      "desde tablero.html (tarjeta 'Wi-Fi de la placa', por USB) o en "
                      "idf.py menuconfig. Reinicio en 20 s...");
        vTaskDelay(pdMS_TO_TICKS(20000));
        esp_restart();
    }
#if WIFI_SIEMPRE_ENCENDIDO
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_LOGI(TAG, "Ahorro de energia del Wi-Fi desactivado (radio siempre encendida)");
#endif

    // Configuracion del cliente MQTT
    ESP_LOGI(TAG, "Broker: %s | canal: %s", TS_BROKER_URI, TS_CHANNEL_ID);
    esp_mqtt_client_config_t configuracion_mqtt = {
        .broker.address.uri = TS_BROKER_URI,
        .credentials.client_id = TS_CLIENT_ID,
        .credentials.username = TS_USERNAME,
        .credentials.authentication.password = TS_PASSWORD,
    };

    cliente_mqtt = esp_mqtt_client_init(&configuracion_mqtt);
    esp_mqtt_client_register_event(cliente_mqtt, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(cliente_mqtt);
}
