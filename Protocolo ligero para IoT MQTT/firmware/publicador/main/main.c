/*
 * main.c - Publicador MQTT: temperatura del ESP32-C6 -> ThingSpeak
 * ============================================================================
 * Practica: Protocolo ligero para IoT MQTT - Ejercicio 1 (ESP-IDF v6.1)
 * ============================================================================
 *
 * QUE HACE
 *   1. Conecta la placa a la red Wi-Fi con wifi_red_conectar() (componente
 *      firmware/wifi_red): usa la red guardada en la placa, que se cambia por
 *      USB desde tablero.html, o si no hay ninguna la de "idf.py menuconfig".
 *   2. Se conecta como cliente MQTT al broker de ThingSpeak,
 *      mqtt3.thingspeak.com, por el puerto 1883 (sin cifrar).
 *   3. Cada 20 s lee el sensor de temperatura integrado en el chip (mediana
 *      de 5 lecturas) y publica el valor en el campo 1 (field1) del canal:
 *
 *          topico:  channels/<CHANNEL_ID>/publish
 *          mensaje: field1=31.25&status=MQTTPublish
 *
 * CAMBIOS RESPECTO AL CODIGO DEL PDF
 *   - URI del broker. El PDF trae "mqtt://://thingspeak.com": el nombre del
 *     servidor se perdio al copiar el texto. El correcto es
 *     mqtt://mqtt3.thingspeak.com.
 *   - El PDF publica un valor fijo (25) una sola vez, al conectar. Aqui se
 *     publica la temperatura real cada 20 s (punto 2.1 de la practica), desde
 *     una tarea de FreeRTOS que espera a que haya conexion con el broker.
 *     Un canal gratuito de ThingSpeak admite una actualizacion cada 15 s como
 *     maximo; los mensajes que llegan antes se descartan sin aviso.
 *   - Si el Wi-Fi no conecta, el programa lo explica y reinicia la placa en
 *     lugar de detenerse con un ESP_ERROR_CHECK.
 *   - Las credenciales se leen de firmware/credenciales.h, que no se sube al
 *     repositorio.
 *
 * SENSOR DE TEMPERATURA
 *   El ESP32-C6 lleva un sensor de temperatura integrado (driver
 *   temperature_sensor, componente esp_driver_tsens). Mide la temperatura del
 *   silicio, no la del ambiente: da valores mas altos que un termometro y
 *   sube con la actividad del chip, por ejemplo al usar el Wi-Fi. Con el
 *   rango -10..80 C el driver elige su escala de menor error (< 1 C).
 *
 * LO QUE SE APRENDIO EN LA PLACA
 *   En 10 minutos de pruebas, una de las 30 lecturas salio a 55,8 C entre
 *   otras de 26,8 y 31,8 C. El salto coincide con un escalon del DAC del
 *   sensor (27,88 C): la radio Wi-Fi usa el mismo sensor para calibrarse y,
 *   si lo cambia de escala justo cuando el driver lee, la conversion sale
 *   desviada. Por eso cada muestra es la mediana de 5 lecturas, que descarta
 *   una lectura aislada erronea.
 */
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "wifi_red.h"                   // Wi-Fi: red guardada en NVS, cambiable por USB

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "driver/temperature_sensor.h"  // Sensor de temperatura integrado en el chip

#include "credenciales.h"               // TS_CHANNEL_ID, TS_CLIENT_ID, TS_USERNAME, TS_PASSWORD

static const char *TAG = "MQTT_THINGSPEAK";

/* ------------------------------------------------------ Configuracion -- */

// Broker MQTT de ThingSpeak. "mqtt://" usa el puerto 1883, sin cifrar.
// credenciales.h puede definir otro (ver credenciales.broker_publico.h).
#ifndef TS_BROKER_URI
#define TS_BROKER_URI "mqtt://mqtt3.thingspeak.com"
#endif

// Cada cuanto se publica la temperatura. ThingSpeak gratuito: 15 s minimo.
#define PERIODO_PUBLICACION_MS  20000

// Rango de temperatura esperado en el chip, en grados C. Con el, el driver
// elige la escala del sensor: -10..80 es la de menor error (< 1 C).
#define TEMP_MIN_C  -10
#define TEMP_MAX_C   80

// Cada muestra es la mediana de varias lecturas seguidas (ver la cabecera).
// Una lectura que se aleja mas de LIMITE_ANOMALIA_C de la mediana se
// descarta y se avisa por el monitor.
#define LECTURAS_POR_MUESTRA     5
#define PAUSA_ENTRE_LECTURAS_MS  10
#define LIMITE_ANOMALIA_C        5.0f

/* ------------------------------------------------------------ Estado -- */

static esp_mqtt_client_handle_t client;
static temperature_sensor_handle_t sensor_temp;

// Bit "conectado al broker". Lo ponen y lo quitan los eventos MQTT, y la
// tarea de publicacion espera a que este puesto.
static EventGroupHandle_t eventos_mqtt;
#define BIT_CONECTADO  BIT0

/* -------------------------------------------------------------- MQTT -- */

// Detalle de un MQTT_EVENT_ERROR. Con ThingSpeak, el error tipico es que el
// broker rechace la conexion porque las credenciales no son correctas.
static void informar_error(const esp_mqtt_error_codes_t *error)
{
    if (error->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
        ESP_LOGE(TAG, "El broker rechazo la conexion (codigo %d). Revise TS_CLIENT_ID, "
                      "TS_USERNAME y TS_PASSWORD en credenciales.h",
                 error->connect_return_code);
    } else if (error->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
        // P. ej. ESP_ERR_ESP_TLS_CANNOT_RESOLVE_HOSTNAME si fallo el DNS
        ESP_LOGE(TAG, "Fallo de la conexion con el broker: %s (errno %d)",
                 esp_err_to_name(error->esp_tls_last_esp_err), error->esp_transport_sock_errno);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Conectado exitosamente al Broker");
            xEventGroupSetBits(eventos_mqtt, BIT_CONECTADO);   // la tarea publica ya
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Desconectado del Broker. Intentando reconexion...");
            xEventGroupClearBits(eventos_mqtt, BIT_CONECTADO);
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "Error en el evento MQTT");
            informar_error(event->error_handle);
            break;

        default:
            break;
    }
}

/* ------------------------------------------------------------ Sensor -- */

static int comparar_float(const void *a, const void *b)
{
    float x = *(const float *)a, y = *(const float *)b;
    return (x > y) - (x < y);
}

// Lee el sensor LECTURAS_POR_MUESTRA veces y devuelve la mediana. Las
// lecturas que fallan o que se alejan de la mediana se descartan con aviso.
static esp_err_t leer_temperatura(float *mediana)
{
    float v[LECTURAS_POR_MUESTRA];
    int n = 0;
    for (int i = 0; i < LECTURAS_POR_MUESTRA; i++) {
        if (temperature_sensor_get_celsius(sensor_temp, &v[n]) == ESP_OK) {
            n++;
        }
        if (i + 1 < LECTURAS_POR_MUESTRA) {
            vTaskDelay(pdMS_TO_TICKS(PAUSA_ENTRE_LECTURAS_MS));
        }
    }
    if (n < 3) {
        return ESP_FAIL;            // demasiadas lecturas fallidas
    }
    qsort(v, n, sizeof(float), comparar_float);
    *mediana = v[n / 2];
    for (int i = 0; i < n; i++) {
        if (fabsf(v[i] - *mediana) > LIMITE_ANOMALIA_C) {
            ESP_LOGW(TAG, "Lectura anomala descartada: %.2f C (mediana %.2f C)", v[i], *mediana);
        }
    }
    return ESP_OK;
}

// Lee el sensor y publica la temperatura en el campo 1 del canal. Es el
// codigo que el PDF ejecuta al conectar, con la lectura real en lugar del
// valor fijo de 25 C.
static void publicar_temperatura(void)
{
    float temperatura;
    esp_err_t err = leer_temperatura(&temperatura);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo leer el sensor de temperatura: %s", esp_err_to_name(err));
        return;
    }

    // Creamos el payload en formato que acepta ThingSpeak (URL encoded)
    char payload[64];
    snprintf(payload, sizeof(payload), "field1=%.2f&status=MQTTPublish", temperatura);

    // Construimos el topico correcto
    char topic[64];
    snprintf(topic, sizeof(topic), "channels/%s/publish", TS_CHANNEL_ID);

    // Publicamos: QoS=0, Retain=0 (ThingSpeak solo admite QoS 0). Con QoS 0
    // el ID devuelto es 0 si el mensaje salio y negativo si hubo un error.
    int msg_id = esp_mqtt_client_publish(client, topic, payload, 0, 0, 0);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "No se pudo publicar (%d)", msg_id);
    } else {
        ESP_LOGI(TAG, "Temperatura %.2f C -> %s: %s (ID %d)", temperatura, topic, payload, msg_id);
    }
}

/* ------------------------------------------------------------ Tareas -- */

// Publica la temperatura cada PERIODO_PUBLICACION_MS. Mientras no hay
// conexion con el broker, la tarea queda bloqueada sin gastar CPU.
static void tarea_publicar(void *arg)
{
    for (;;) {
        xEventGroupWaitBits(eventos_mqtt, BIT_CONECTADO, pdFALSE, pdTRUE, portMAX_DELAY);
        publicar_temperatura();
        vTaskDelay(pdMS_TO_TICKS(PERIODO_PUBLICACION_MS));
    }
}

/* ----------------------------------------------------------- app_main -- */

void app_main(void)
{
    ESP_LOGI(TAG, "Iniciando ESP32-C6...");

    // Sensor de temperatura integrado. Primera lectura antes de encender el
    // Wi-Fi, para comprobar que el sensor responde.
    temperature_sensor_config_t cfg_temp = TEMPERATURE_SENSOR_CONFIG_DEFAULT(TEMP_MIN_C, TEMP_MAX_C);
    ESP_ERROR_CHECK(temperature_sensor_install(&cfg_temp, &sensor_temp));
    ESP_ERROR_CHECK(temperature_sensor_enable(sensor_temp));
    float temperatura;
    ESP_ERROR_CHECK(temperature_sensor_get_celsius(sensor_temp, &temperatura));
    ESP_LOGI(TAG, "Sensor de temperatura listo: %.2f C (Wi-Fi apagado)", temperatura);

    if (strncmp(TS_CHANNEL_ID, "TU_", 3) == 0 || strncmp(TS_CLIENT_ID, "TU_", 3) == 0) {
        ESP_LOGW(TAG, "credenciales.h aun tiene los valores de la plantilla: "
                      "el broker rechazara la conexion");
    }

    // Inicializar memoria NVS requerida por el Wi-Fi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Inicializar interfaz de red y bucle de eventos
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

    // Configuracion del cliente MQTT de Espressif
    ESP_LOGI(TAG, "Broker: %s | canal: %s", TS_BROKER_URI, TS_CHANNEL_ID);
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = TS_BROKER_URI,
        .credentials.client_id = TS_CLIENT_ID,
        .credentials.username = TS_USERNAME,
        .credentials.authentication.password = TS_PASSWORD,
    };

    eventos_mqtt = xEventGroupCreate();
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    // Tarea que publica la temperatura periodicamente
    xTaskCreate(tarea_publicar, "publicar", 4096, NULL, 5, NULL);
}
