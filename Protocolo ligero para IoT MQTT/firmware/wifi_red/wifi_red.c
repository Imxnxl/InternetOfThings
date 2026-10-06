/*
 * wifi_red.c - Wi-Fi con la red guardada en la placa, cambiable por USB
 * Ver wifi_red.h. Lo usan el publicador y el suscriptor.
 */
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs.h"
#include "driver/uart.h"
#include "driver/uart_vfs.h"
#include "sdkconfig.h"

#include "wifi_red.h"

static const char *TAG = "WIFI_RED";

// Red de menuconfig (Example Connection Configuration). Solo se usa si no hay
// otra guardada en NVS.
#ifdef CONFIG_EXAMPLE_WIFI_SSID
#define SSID_MENUCONFIG   CONFIG_EXAMPLE_WIFI_SSID
#define CLAVE_MENUCONFIG  CONFIG_EXAMPLE_WIFI_PASSWORD
#else
#define SSID_MENUCONFIG   ""
#define CLAVE_MENUCONFIG  ""
#endif

#define NVS_ESPACIO        "wifi_red"
#define REINTENTOS_MAX     6           // como CONFIG_EXAMPLE_WIFI_CONN_MAX_RETRY
#define ESPERA_IP_MS       30000
#define UART_CONSOLA       CONFIG_ESP_CONSOLE_UART_NUM
#define LINEA_MAX          200

#define BIT_IP     BIT0
#define BIT_FALLO  BIT1

/* ------------------------------------------------------------ Estado -- */

static char ssid_en_uso[33];
static const char *origen = "menuconfig";       // "nvs" o "menuconfig"
static volatile bool conectado = false;
static char ip_texto[16] = "-";
static int reintentos = 0;
static int ultimo_motivo = 0;
static EventGroupHandle_t grupo;

/* ------------------------------------------------------- NVS: la red -- */

// Lee la red guardada. Devuelve false si no hay ninguna.
static bool leer_red_guardada(char *ssid, size_t tam_ssid, char *clave, size_t tam_clave)
{
    nvs_handle_t h;
    if (nvs_open(NVS_ESPACIO, NVS_READONLY, &h) != ESP_OK) return false;
    bool ok = nvs_get_str(h, "ssid", ssid, &tam_ssid) == ESP_OK &&
              nvs_get_str(h, "clave", clave, &tam_clave) == ESP_OK &&
              ssid[0] != '\0';
    nvs_close(h);
    return ok;
}

static esp_err_t guardar_red(const char *ssid, const char *clave)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_ESPACIO, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_str(h, "ssid", ssid);
    if (err == ESP_OK) err = nvs_set_str(h, "clave", clave);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

static esp_err_t borrar_red(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_ESPACIO, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    nvs_erase_all(h);
    err = nvs_commit(h);
    nvs_close(h);
    return err;
}

/* ---------------------------------------------- Comandos por consola -- */

static void informar_estado(void)
{
    printf("WIFI_ESTADO ssid=\"%s\" origen=%s conectado=%s ip=%s\n",
           ssid_en_uso, origen, conectado ? "si" : "no", ip_texto);
}

static const char *explicar_motivo(int motivo)
{
    switch (motivo) {
        case WIFI_REASON_NO_AP_FOUND:              return "no se encuentra la red (nombre mal escrito o red de 5 GHz)";
        case WIFI_REASON_AUTH_FAIL:
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_HANDSHAKE_TIMEOUT:        return "contrasena incorrecta";
        case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY:
        case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD: return "la seguridad de la red no es compatible";
        default:                                   return "ver el codigo en la documentacion de ESP-IDF";
    }
}

static void procesar_linea(char *linea)
{
    if (strcmp(linea, "WIFI?") == 0) {
        informar_estado();
        return;
    }
    if (strcmp(linea, "WIFI_BORRAR") == 0) {
        if (borrar_red() == ESP_OK) {
            printf("WIFI_OK red guardada borrada: se usara la de menuconfig. Reinicio...\n");
            vTaskDelay(pdMS_TO_TICKS(300));
            esp_restart();
        }
        printf("WIFI_ERROR no se pudo borrar la red guardada\n");
        return;
    }
    if (strncmp(linea, "WIFI_SET\t", 9) == 0) {
        char *ssid = linea + 9;
        char *tab = strchr(ssid, '\t');
        char *clave = "";
        if (tab) { *tab = '\0'; clave = tab + 1; }
        size_t ls = strlen(ssid), lc = strlen(clave);
        if (ls == 0 || ls > 32) {
            printf("WIFI_ERROR el nombre de la red debe tener de 1 a 32 caracteres\n");
            return;
        }
        if (lc != 0 && (lc < 8 || lc > 63)) {
            printf("WIFI_ERROR la contrasena debe tener de 8 a 63 caracteres (o estar vacia si la red es abierta)\n");
            return;
        }
        if (guardar_red(ssid, clave) != ESP_OK) {
            printf("WIFI_ERROR no se pudo guardar en la memoria NVS\n");
            return;
        }
        printf("WIFI_OK red \"%s\" guardada. Reinicio para conectarme...\n", ssid);
        vTaskDelay(pdMS_TO_TICKS(300));
        esp_restart();
    }
    // Cualquier otra linea se ignora.
}

// Lee la consola caracter a caracter y procesa cada linea completa.
static void tarea_consola(void *arg)
{
    char linea[LINEA_MAX + 1];
    size_t n = 0;
    for (;;) {
        uint8_t c;
        if (uart_read_bytes(UART_CONSOLA, &c, 1, portMAX_DELAY) != 1) continue;
        if (c == '\r') continue;
        if (c == '\n') {
            linea[n] = '\0';
            if (n > 0) procesar_linea(linea);
            n = 0;
        } else if (n < LINEA_MAX) {
            linea[n++] = (char)c;
        }
    }
}

static void iniciar_consola(void)
{
    if (!uart_is_driver_installed(UART_CONSOLA)) {
        ESP_ERROR_CHECK(uart_driver_install(UART_CONSOLA, 512, 0, 0, NULL, 0));
        uart_vfs_dev_use_driver(UART_CONSOLA);
    }
    xTaskCreate(tarea_consola, "wifi_consola", 4096, NULL, 4, NULL);
}

/* ------------------------------------------------------------- Wi-Fi -- */

static void manejador(void *arg, esp_event_base_t base, int32_t id, void *datos)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *d = datos;
        conectado = false;
        strcpy(ip_texto, "-");
        ultimo_motivo = d->reason;
        if (reintentos < REINTENTOS_MAX) {
            reintentos++;
            ESP_LOGW(TAG, "Wi-Fi desconectado (motivo %d), reintento %d de %d",
                     d->reason, reintentos, REINTENTOS_MAX);
            esp_wifi_connect();
        } else {
            xEventGroupSetBits(grupo, BIT_FALLO);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = datos;
        snprintf(ip_texto, sizeof(ip_texto), IPSTR, IP2STR(&e->ip_info.ip));
        conectado = true;
        reintentos = 0;
        xEventGroupSetBits(grupo, BIT_IP);
        informar_estado();
    }
}

esp_err_t wifi_red_conectar(void)
{
    // Primero los comandos: asi la red se puede corregir aunque no conecte.
    iniciar_consola();

    char ssid[33] = "", clave[65] = "";
    if (leer_red_guardada(ssid, sizeof(ssid), clave, sizeof(clave))) {
        origen = "nvs";
    } else {
        origen = "menuconfig";
        strlcpy(ssid, SSID_MENUCONFIG, sizeof(ssid));
        strlcpy(clave, CLAVE_MENUCONFIG, sizeof(clave));
    }
    strlcpy(ssid_en_uso, ssid, sizeof(ssid_en_uso));

    if (ssid[0] == '\0') {
        printf("WIFI_ERROR no hay ninguna red configurada: enviela desde tablero.html\n");
        return ESP_FAIL;
    }

    grupo = xEventGroupCreate();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, manejador, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, manejador, NULL));

    wifi_config_t wcfg = {
        .sta = {
            .scan_method = WIFI_ALL_CHANNEL_SCAN,
            // Los puntos de acceso de los celulares suelen usar WPA2 o WPA3:
            // se aceptan los dos.
            .threshold.authmode = clave[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };
    strlcpy((char *)wcfg.sta.ssid, ssid, sizeof(wcfg.sta.ssid));
    strlcpy((char *)wcfg.sta.password, clave, sizeof(wcfg.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wcfg));
    printf("WIFI_ESTADO ssid=\"%s\" origen=%s conectado=no ip=- (conectando...)\n", ssid, origen);
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(grupo, BIT_IP | BIT_FALLO, pdFALSE, pdFALSE,
                                           pdMS_TO_TICKS(ESPERA_IP_MS));
    if (bits & BIT_IP) return ESP_OK;

    printf("WIFI_ERROR no se pudo conectar a \"%s\" (motivo %d: %s)\n",
           ssid, ultimo_motivo, explicar_motivo(ultimo_motivo));
    return ESP_FAIL;
}
