/*
 * led_temperatura.c - LED RGB que muestra la temperatura recibida por MQTT
 * Ver led_temperatura.h. Lo usan el publicador y el suscriptor.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "driver/gpio.h"

#include "led_temperatura.h"

static const char *TAG = "LED_TEMPERATURA";

/* ------------------------------------------------------ Configuracion -- */

// Pines del modulo LED RGB (catodo comun: un 1 enciende el color)
#define PIN_LED_ROJO   GPIO_NUM_19
#define PIN_LED_VERDE  GPIO_NUM_20
#define PIN_LED_AZUL   GPIO_NUM_21

// Limites de los colores, en grados C
#define TEMPERATURA_MAXIMA_AZUL_C   30.0f   // 30 C o menos            -> azul
#define TEMPERATURA_MINIMA_ROJO_C   50.0f   // 50 C o mas              -> rojo
                                            // entre los dos (p. ej. 40 C) -> verde

// Temperaturas fuera de este rango se consideran un dato erroneo
#define TEMPERATURA_MINIMA_VALIDA_C  -50.0f
#define TEMPERATURA_MAXIMA_VALIDA_C  150.0f

typedef enum { LED_APAGADO, LED_AZUL, LED_VERDE, LED_ROJO } color_del_led_t;

/* --------------------------------------------------------------- LED -- */

static void encender_color(color_del_led_t color)
{
    gpio_set_level(PIN_LED_ROJO,  color == LED_ROJO);
    gpio_set_level(PIN_LED_VERDE, color == LED_VERDE);
    gpio_set_level(PIN_LED_AZUL,  color == LED_AZUL);
}

static const char *nombre_del_color(color_del_led_t color)
{
    switch (color) {
        case LED_AZUL:  return "azul";
        case LED_VERDE: return "verde";
        case LED_ROJO:  return "rojo";
        default:        return "apagado";
    }
}

// La regla de la practica: 30 C o menos azul, 50 C o mas rojo, y verde entre
// los dos.
static color_del_led_t color_segun_temperatura(float temperatura_c)
{
    if (temperatura_c <= TEMPERATURA_MAXIMA_AZUL_C) {
        return LED_AZUL;
    }
    if (temperatura_c >= TEMPERATURA_MINIMA_ROJO_C) {
        return LED_ROJO;
    }
    return LED_VERDE;
}

void led_temperatura_iniciar(void)
{
    gpio_config_t configuracion_pines = {
        .pin_bit_mask = (1ULL << PIN_LED_ROJO) | (1ULL << PIN_LED_VERDE) | (1ULL << PIN_LED_AZUL),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&configuracion_pines));
    // Fuerza de salida minima (~5 mA), por si el modulo no lleva resistencias,
    // como en las practicas anteriores.
    gpio_set_drive_capability(PIN_LED_ROJO,  GPIO_DRIVE_CAP_0);
    gpio_set_drive_capability(PIN_LED_VERDE, GPIO_DRIVE_CAP_0);
    gpio_set_drive_capability(PIN_LED_AZUL,  GPIO_DRIVE_CAP_0);
    encender_color(LED_APAGADO);          // apagado hasta la primera temperatura
}

void led_temperatura_mostrar(float temperatura_c)
{
    color_del_led_t color = color_segun_temperatura(temperatura_c);
    encender_color(color);
    ESP_LOGI(TAG, "Temperatura recibida: %.2f C -> LED %s", temperatura_c, nombre_del_color(color));
}

/* ----------------------------------------------- Lectura del mensaje -- */

// Copia en 'valor' el contenido de field1, venga en JSON o como formulario:
//   {"field1":"40", ...}      -> "40"     (JSON de ThingSpeak y del tablero)
//   field1=40&status=...      -> "40"     (lo que publica el publicador)
//   {"field1":null, ...}      -> false    (esa entrada no trae field1)
static bool buscar_field1(const char *texto, char *valor, size_t tamano_valor)
{
    const char *inicio = strstr(texto, "\"field1\":");
    char fin_del_valor[3] = "&";                    // formulario: hasta el siguiente '&'

    if (inicio != NULL) {                           // JSON
        inicio += strlen("\"field1\":");
        while (*inicio == ' ') {
            inicio++;
        }
        if (strncmp(inicio, "null", 4) == 0) {
            return false;
        }
        if (*inicio == '"') {                       // "40": hasta la comilla
            inicio++;
            strcpy(fin_del_valor, "\"");
        } else {                                    // 40: hasta la coma o la llave
            strcpy(fin_del_valor, ",}");
        }
    } else {                                        // formulario
        inicio = strstr(texto, "field1=");
        if (inicio == NULL || (inicio != texto && inicio[-1] != '&')) {
            return false;
        }
        inicio += strlen("field1=");
    }

    size_t largo_valor = strcspn(inicio, fin_del_valor);
    if (largo_valor == 0 || largo_valor >= tamano_valor) {
        return false;
    }
    memcpy(valor, inicio, largo_valor);
    valor[largo_valor] = '\0';
    return true;
}

bool led_temperatura_procesar_mensaje(const char *datos, int largo)
{
    // event->data no termina en '\0': se copia a un texto normal de C
    char texto_mensaje[512];
    int largo_copiado = largo < (int)sizeof(texto_mensaje) - 1 ? largo : (int)sizeof(texto_mensaje) - 1;
    memcpy(texto_mensaje, datos, largo_copiado);
    texto_mensaje[largo_copiado] = '\0';

    char valor_field1[16];
    if (!buscar_field1(texto_mensaje, valor_field1, sizeof(valor_field1))) {
        ESP_LOGI(TAG, "El mensaje no trae temperatura (field1): el LED no cambia");
        return false;
    }

    char *resto_sin_leer;
    float temperatura_c = strtof(valor_field1, &resto_sin_leer);
    bool es_numero = (resto_sin_leer != valor_field1 && *resto_sin_leer == '\0');
    if (!es_numero || temperatura_c < TEMPERATURA_MINIMA_VALIDA_C ||
        temperatura_c > TEMPERATURA_MAXIMA_VALIDA_C) {
        ESP_LOGW(TAG, "field1=%s no es una temperatura valida: el LED no cambia", valor_field1);
        return false;
    }

    led_temperatura_mostrar(temperatura_c);
    return true;
}
