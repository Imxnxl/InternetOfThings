/*
 * main.c - Suscriptor MQTT: recibe al instante los datos del canal de ThingSpeak
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
 *   3. Cada vez que el canal recibe un dato (desde el navegador, desde otro
 *      dispositivo o desde el publicador), el broker se lo reenvia a la placa
 *      y salta el evento MQTT_EVENT_DATA, sin que la placa pregunte nada
 *      (sin polling). Se imprimen el topico y el mensaje.
 *   4. Accion: si field1 vale 1 se enciende el LED de GPIO20, y si vale 0 se
 *      apaga. Es la accion "opcional" que propone el PDF.
 *
 * CAMBIOS RESPECTO AL CODIGO DEL PDF
 *   - URI del broker: mqtt://mqtt3.thingspeak.com (igual que en el publicador).
 *   - MQTT_EVENT_SUBCRIBED -> MQTT_EVENT_SUBSCRIBED. Con la errata del PDF el
 *     programa no compila.
 *   - ThingSpeak no reenvia el texto "field1=1" que se envio, sino la entrada
 *     completa del canal en formato JSON ("field1":"1", ...). Por eso la
 *     comparacion strncmp(event->data, "field1=1", 8) del PDF nunca se
 *     cumple. Aqui se busca el valor de "field1" dentro del JSON.
 *   - Se comprueba la respuesta del broker a la suscripcion: si el dispositivo
 *     MQTT no tiene permiso de suscripcion en el canal, el broker la rechaza.
 *
 * CONEXIONES (opcional, solo para ver la accion en un LED)
 *   LED RGB   G     -> GPIO20   (el mismo modulo de las practicas anteriores)
 *             comun -> GND      (catodo comun)
 *   Sin LED, la accion se ve igualmente en el monitor.
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
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "driver/gpio.h"

#include "credenciales.h"               // TS_CHANNEL_ID, TS_CLIENT_ID, TS_USERNAME, TS_PASSWORD

static const char *TAG = "MQTT_SUB_THINGSPEAK";

/* ------------------------------------------------------ Configuracion -- */

// Broker MQTT de ThingSpeak. "mqtt://" usa el puerto 1883, sin cifrar.
// credenciales.h puede definir otro (ver credenciales.broker_publico.h).
#ifndef TS_BROKER_URI
#define TS_BROKER_URI "mqtt://mqtt3.thingspeak.com"
#endif

// LED que enciende el comando field1=1 (canal verde del modulo RGB).
#define PIN_LED  GPIO_NUM_20

// Ahorro de energia del Wi-Fi. Con 0 se deja el de ESP-IDF (WIFI_PS_MIN_MODEM):
// la radio duerme entre las balizas del router, que guarda los mensajes que
// llegan para la placa hasta la siguiente baliza. Gasta menos, pero cada
// comando tarda mas en llegar (seccion 5.4 del informe). Con 1 la radio queda
// siempre encendida.
#define WIFI_SIEMPRE_ENCENDIDO  0

static esp_mqtt_client_handle_t client;

/* ------------------------------------------------------ Mensaje JSON -- */

// Busca "campo" en el JSON que envia ThingSpeak y copia su valor en 'valor'.
//   "field1":"25"  -> "25"   (ThingSpeak envia los campos como texto)
//   "field1":null  -> false  (el campo no cambio en esta entrada)
// 'json' tiene que terminar en '\0'.
static bool valor_de_campo(const char *json, const char *campo, char *valor, size_t tam)
{
    char clave[24];
    snprintf(clave, sizeof(clave), "\"%s\":", campo);
    const char *p = strstr(json, clave);
    if (p == NULL) {
        return false;
    }
    p += strlen(clave);
    while (*p == ' ') {
        p++;
    }
    if (strncmp(p, "null", 4) == 0) {
        return false;
    }

    bool entre_comillas = (*p == '"');
    if (entre_comillas) {
        p++;
    }
    size_t n = 0;
    while (p[n] != '\0' && n + 1 < tam &&
           (entre_comillas ? p[n] != '"' : (p[n] != ',' && p[n] != '}'))) {
        valor[n] = p[n];
        n++;
    }
    valor[n] = '\0';
    return n > 0;
}

// Accion sobre el mensaje recibido: field1 = 1 enciende el LED y field1 = 0
// lo apaga. event->data no termina en '\0', asi que primero se copia a un
// buffer para poder usar las funciones de texto de C.
static void procesar_mensaje(const char *datos, int largo)
{
    char json[512];
    int n = largo < (int)sizeof(json) - 1 ? largo : (int)sizeof(json) - 1;
    memcpy(json, datos, n);
    json[n] = '\0';

    char valor[16];
    if (!valor_de_campo(json, "field1", valor, sizeof(valor))) {
        ESP_LOGI(TAG, "Esta entrada no trae field1: no hay accion");
        return;
    }

    char *fin;
    float numero = strtof(valor, &fin);
    bool es_numero = (fin != valor && *fin == '\0');
    if (es_numero && numero == 1.0f) {
        gpio_set_level(PIN_LED, 1);
        ESP_LOGI(TAG, "Accion: Comando de encendido detectado (field1=%s) -> LED encendido", valor);
    } else if (es_numero && numero == 0.0f) {
        gpio_set_level(PIN_LED, 0);
        ESP_LOGI(TAG, "Accion: Comando de apagado detectado (field1=%s) -> LED apagado", valor);
    } else {
        ESP_LOGI(TAG, "field1=%s no es un comando (1 o 0): el LED no cambia", valor);
    }
}

/* -------------------------------------------------------------- MQTT -- */

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    char topic_buffer[128];

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED: {
            ESP_LOGI(TAG, "Conectado al Broker. Suscribiendose al canal...");

            // Construimos el topico de suscripcion para ThingSpeak
            snprintf(topic_buffer, sizeof(topic_buffer), "channels/%s/subscribe", TS_CHANNEL_ID);

            // Nos suscribimos con QoS 0. Se hace en cada conexion: si el broker
            // corta y el cliente reconecta, la suscripcion se renueva sola.
            int msg_id = esp_mqtt_client_subscribe(client, topic_buffer, 0);
            ESP_LOGI(TAG, "Suscripcion a %s enviada con exito, ID de mensaje: %d",
                     topic_buffer, msg_id);
            break;
        }

        case MQTT_EVENT_SUBSCRIBED:
            // El broker responde con un codigo por topico (0x00 = aceptada con
            // QoS 0; 0x80 = rechazada). El cliente lo traduce a error_type.
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_SUBSCRIBE_FAILED) {
                ESP_LOGE(TAG, "El broker rechazo la suscripcion. En ThingSpeak, el "
                              "dispositivo MQTT necesita 'Allow Subscribe' en este canal");
            } else {
                ESP_LOGI(TAG, "Suscripcion confirmada por el Broker. Esperando datos...");
            }
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "NOTIFICACION RECIBIDA!");

            // Imprimir el topico de donde provienen los datos
            printf("Topico: %.*s\r\n", event->topic_len, event->topic);

            // Imprimir el contenido del mensaje (Payload)
            printf("Datos recibidos: %.*s\r\n", event->data_len, event->data);

            // Parsear el mensaje y ejecutar la accion sobre el LED
            procesar_mensaje(event->data, event->data_len);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Desconectado del Broker.");
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "Error en el entorno MQTT");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                ESP_LOGE(TAG, "El broker rechazo la conexion (codigo %d). Revise TS_CLIENT_ID, "
                              "TS_USERNAME y TS_PASSWORD en credenciales.h",
                         event->error_handle->connect_return_code);
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

    // LED de la accion, apagado al arrancar. Fuerza de salida minima (~5 mA)
    // por si el modulo no lleva resistencia, como en las practicas anteriores.
    gpio_reset_pin(PIN_LED);
    gpio_set_direction(PIN_LED, GPIO_MODE_OUTPUT);
    gpio_set_drive_capability(PIN_LED, GPIO_DRIVE_CAP_0);
    gpio_set_level(PIN_LED, 0);

    if (strncmp(TS_CHANNEL_ID, "TU_", 3) == 0 || strncmp(TS_CLIENT_ID, "TU_", 3) == 0) {
        ESP_LOGW(TAG, "credenciales.h aun tiene los valores de la plantilla: "
                      "el broker rechazara la conexion");
    }

    // Inicializacion del almacenamiento NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Inicializar la pila de red nativa
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Conexion Wi-Fi (Recuerda configurar SSID/Password mediante idf.py menuconfig)
    if (example_connect() != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo conectar al Wi-Fi. Revise el SSID y la contrasena en "
                      "idf.py menuconfig -> Example Connection Configuration "
                      "(solo redes de 2,4 GHz). Reinicio en 10 s...");
        vTaskDelay(pdMS_TO_TICKS(10000));
        esp_restart();
    }
#if WIFI_SIEMPRE_ENCENDIDO
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_LOGI(TAG, "Ahorro de energia del Wi-Fi desactivado (radio siempre encendida)");
#endif

    // Configuracion del cliente MQTT
    ESP_LOGI(TAG, "Broker: %s | canal: %s", TS_BROKER_URI, TS_CHANNEL_ID);
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = TS_BROKER_URI,
        .credentials.client_id = TS_CLIENT_ID,
        .credentials.username = TS_USERNAME,
        .credentials.authentication.password = TS_PASSWORD,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}
