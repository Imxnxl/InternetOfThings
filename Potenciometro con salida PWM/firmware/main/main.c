/*
 * main.c - Potenciometro (ADC) -> brillo de un LED (PWM) - ESP32-C6 con ESP-IDF
 * ============================================================================
 *
 * QUE HACE
 *   Lee la tension del cursor de un potenciometro con el ADC, con la
 *   resolucion por defecto de 12 bits, y con esa lectura fija el ciclo de
 *   trabajo de una senal PWM de 12 bits que regula el brillo de un LED:
 *
 *       cursor a 0 V    -> PWM   0 % -> LED apagado
 *       cursor a 1,65 V -> PWM  50 % -> LED a media potencia
 *       cursor a 3,3 V  -> PWM 100 % -> LED al maximo
 *
 * DONDE ESTA main()
 *   Un programa de ESP-IDF no empieza en main(), sino en app_main(). El
 *   arranque de ESP-IDF hace el papel de main(): inicializa el chip, crea
 *   una tarea de FreeRTOS llamada "main", arranca el planificador y esa
 *   tarea llama a app_main() (components/freertos/app_startup.c). Por eso
 *   app_main() es, a todos los efectos, el main() de este programa.
 *
 * LO QUE SE APRENDIO EN LA PLACA
 *   La idea obvia es pasar la lectura cruda (0..4095) directamente al PWM.
 *   Pero con la atenuacion de 12 dB, el ADC del ESP32-C6 no asigna 4095 a
 *   3,3 V: el cursor a 3,31 V da ~3433 cuentas, y el LED se quedaba en el
 *   84 %. Por eso el ciclo de trabajo se calcula con la tension calibrada
 *   (adc_cali_raw_to_voltage). La version directa sigue disponible en
 *   menuconfig para comparar (PRACTICA_TENSION_CALIBRADA).
 *
 * CONEXIONES
 *   Potenciometro  pata izquierda -> GND
 *                  pata central   -> GPIO2  (ADC1, canal 2)
 *                  pata derecha   -> 3V3    (NUNCA 5V: el C6 admite 3,6 V maximo)
 *   LED RGB        G -> GPIO20 (PWM) | R -> GPIO19 y B -> GPIO21 (apagados)
 *                  comun -> GND (catodo comun; ver PRACTICA_LED_ANODO_COMUN)
 *
 * MONITOR SERIE (idf.py -p COM5 monitor, 115200 baudios)
 *   Cada 100 ms una linea "adc:... mV:... pwm_pct:...". Comandos:
 *     r  prueba de ruido del ADC (con el potenciometro quieto)
 *     i  vuelve a mostrar la configuracion
 */
#include <stdio.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/uart_vfs.h"
#include "esp_chip_info.h"
#include "esp_idf_version.h"
#include "esp_timer.h"
#include "soc/soc_caps.h"
#include "sdkconfig.h"

/* ------------------------------------------------------------- Pines -- */
#define PIN_POT     GPIO_NUM_2
#define PIN_LED_R   GPIO_NUM_19
#define PIN_LED_G   GPIO_NUM_20
#define PIN_LED_B   GPIO_NUM_21

/* ------------------------------------------------------ Configuracion -- */

// Opciones de menuconfig (main/Kconfig.projbuild). Una opcion booleana
// desactivada no aparece en sdkconfig.h, asi que se convierten aqui en 0/1.
#ifdef CONFIG_PRACTICA_TENSION_CALIBRADA
#define USAR_TENSION_CALIBRADA  1
#else
#define USAR_TENSION_CALIBRADA  0
#endif

#ifdef CONFIG_PRACTICA_LED_ANODO_COMUN
#define LED_ANODO_COMUN         1
#else
#define LED_ANODO_COMUN         0
#endif

// ADC: atenuacion de 12 dB (rango 0..3,3 V en el C6) y la resolucion por
// defecto, que en el C6 es de 12 bits: lecturas de 0 a 4095.
#define ADC_ATENUACION      ADC_ATTEN_DB_12
#define ADC_BITS            SOC_ADC_RTC_MAX_BITWIDTH          // 12
#define ADC_MAX             ((1 << ADC_BITS) - 1)             // 4095

// PWM con el periferico LEDC, a 12 bits. En ESP-IDF el ciclo de trabajo
// admite de 0 a 2^12 = 4096, y 4096 es el pin fijo en alto: el 100 %.
#define PWM_BITS            LEDC_TIMER_12_BIT
#define PWM_TOPE            (1 << 12)                         // 4096 = 100 %
#define PWM_MODO            LEDC_LOW_SPEED_MODE
#define PWM_TEMPORIZADOR    LEDC_TIMER_0
#define PWM_CANAL           LEDC_CHANNEL_0

// Tension del cursor que corresponde al 100 % de brillo.
#define TENSION_MAXIMA_MV   3300

// Fuerza de salida de los pines del LED. Segun el manual tecnico del C6
// (campo FUN_DRV de IO_MUX_GPIOn_REG): nivel 0 ~5 mA, 1 ~10 mA, 2 ~20 mA
// (por defecto) y 3 ~40 mA. El nivel 0 limita la corriente por si el LED no
// lleva resistencia en serie.
#define FUERZA_SALIDA_LED   GPIO_DRIVE_CAP_0

// Periodo del bucle principal y del informe por el monitor serie.
#define PERIODO_BUCLE_MS    10
#define PERIODO_INFORME_MS  100

// FreeRTOS en ESP-IDF funciona por defecto a 100 ticks por segundo: un
// retardo de menos de 10 ms seria de 0 ticks y el bucle no cederia nunca el
// nucleo (la tarea Idle se quedaria sin CPU y saltaria el watchdog).
_Static_assert(pdMS_TO_TICKS(PERIODO_BUCLE_MS) >= 1,
               "PERIODO_BUCLE_MS debe durar al menos un tick de FreeRTOS");

#define UART_CONSOLA        CONFIG_ESP_CONSOLE_UART_NUM

/* ----------------------------------------------------- Estado global -- */
static adc_oneshot_unit_handle_t adc;
static adc_cali_handle_t         calibracion;
static adc_channel_t             canal_adc;
static uint32_t                  frecuencia_real_hz;

/* ------------------------------------------------------- Configuracion -- */

// Entrada analogica: unidad ADC1, canal del GPIO2 y calibracion de fabrica.
static void configurar_adc(void)
{
    // El canal se obtiene a partir del pin, en vez de escribirlo a mano.
    adc_unit_t unidad;
    ESP_ERROR_CHECK(adc_oneshot_io_to_channel(PIN_POT, &unidad, &canal_adc));

    adc_oneshot_unit_init_cfg_t cfg_unidad = { .unit_id = unidad };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&cfg_unidad, &adc));

    adc_oneshot_chan_cfg_t cfg_canal = {
        .atten    = ADC_ATENUACION,
        .bitwidth = ADC_BITWIDTH_DEFAULT,      // 12 bits en el C6
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc, canal_adc, &cfg_canal));

    // Calibracion por ajuste de curva: usa los coeficientes que Espressif
    // graba en el eFuse de cada chip para convertir la lectura en mV.
    adc_cali_curve_fitting_config_t cfg_cali = {
        .unit_id  = unidad,
        .chan     = canal_adc,
        .atten    = ADC_ATENUACION,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cfg_cali, &calibracion));
}

// Salida PWM en el canal verde; rojo y azul, apagados.
static void configurar_pwm(void)
{
    ledc_timer_config_t temporizador = {
        .speed_mode      = PWM_MODO,
        .duty_resolution = PWM_BITS,
        .timer_num       = PWM_TEMPORIZADOR,
        .freq_hz         = CONFIG_PRACTICA_PWM_FRECUENCIA_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&temporizador));

    ledc_channel_config_t canal = {
        .gpio_num   = PIN_LED_G,
        .speed_mode = PWM_MODO,
        .channel    = PWM_CANAL,
        .timer_sel  = PWM_TEMPORIZADOR,
        .duty       = 0,
        .hpoint     = 0,
        // En un LED de anodo comun se enciende con nivel bajo: el propio
        // periferico invierte la salida y el resto del programa no cambia.
        .flags.output_invert = LED_ANODO_COMUN,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&canal));

    // Frecuencia que ha conseguido de verdad el temporizador.
    frecuencia_real_hz = ledc_get_freq(PWM_MODO, PWM_TEMPORIZADOR);

    gpio_config_t apagados = {
        .pin_bit_mask = (1ULL << PIN_LED_R) | (1ULL << PIN_LED_B),
        .mode         = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&apagados));
    gpio_set_level(PIN_LED_R, LED_ANODO_COMUN);
    gpio_set_level(PIN_LED_B, LED_ANODO_COMUN);

    // Despues de configurar los pines, que quedan con la fuerza por defecto.
    gpio_set_drive_capability(PIN_LED_R, FUERZA_SALIDA_LED);
    gpio_set_drive_capability(PIN_LED_G, FUERZA_SALIDA_LED);
    gpio_set_drive_capability(PIN_LED_B, FUERZA_SALIDA_LED);
}

// La consola usa la UART0 (el conector "UART" de la placa). Con el driver
// instalado se pueden leer los comandos del monitor serie sin bloquear.
static void configurar_consola(void)
{
    ESP_ERROR_CHECK(uart_driver_install(UART_CONSOLA, 256, 0, 0, NULL, 0));
    uart_vfs_dev_use_driver(UART_CONSOLA);
}

/* ---------------------------------------------------- Entrada y salida -- */

// Una conversion del ADC: lectura cruda y su tension calibrada en mV.
static void leer_potenciometro(int *cruda, int *milivoltios)
{
    ESP_ERROR_CHECK(adc_oneshot_read(adc, canal_adc, cruda));
    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(calibracion, *cruda, milivoltios));
}

// Ciclo de trabajo (0..PWM_TOPE) a partir de la lectura.
static uint32_t calcular_ciclo(int cruda, int milivoltios)
{
#if USAR_TENSION_CALIBRADA
    (void)cruda;
    if (milivoltios < 0) milivoltios = 0;
    if (milivoltios > TENSION_MAXIMA_MV) milivoltios = TENSION_MAXIMA_MV;
    return (uint32_t)milivoltios * PWM_TOPE / TENSION_MAXIMA_MV;
#else
    (void)milivoltios;
    return (uint32_t)cruda;      // la lectura cruda tal cual (0..4095)
#endif
}

static void escribir_brillo(uint32_t ciclo)
{
    ESP_ERROR_CHECK(ledc_set_duty(PWM_MODO, PWM_CANAL, ciclo));
    ESP_ERROR_CHECK(ledc_update_duty(PWM_MODO, PWM_CANAL));
}

/* ------------------------------------------------------- Diagnostico -- */

// Muestra la configuracion tal como ha quedado en el hardware: canal del
// ADC, frecuencia real del PWM y fuerza de salida leida de los registros.
static void informar_configuracion(void)
{
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    gpio_io_config_t led;
    gpio_get_io_config(PIN_LED_G, &led);

    printf("\n==========================================================\n");
    printf(" Potenciometro (ADC) -> brillo de un LED (PWM) - ESP32-C6\n");
    printf(" ESP32-C6 v%d.%d | %d MHz | ESP-IDF %s\n",
           chip.revision / 100, chip.revision % 100,
           CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ, esp_get_idf_version());
    printf("==========================================================\n");
    printf("Entrada  GPIO%d -> ADC1 canal %d | %d bits (0..%d) | atenuacion 12 dB (0..3,3 V)\n",
           PIN_POT, canal_adc, ADC_BITS, ADC_MAX);
    printf("Salida   GPIO%d -> PWM %d Hz pedidos, %lu Hz reales | 12 bits (0..%d = 100 %%)\n",
           PIN_LED_G, CONFIG_PRACTICA_PWM_FRECUENCIA_HZ,
           (unsigned long)frecuencia_real_hz, PWM_TOPE);
#if USAR_TENSION_CALIBRADA
    printf("Ciclo    tension calibrada: 0..%d mV -> 0..%d (el tope de la perilla da el 100 %%)\n",
           TENSION_MAXIMA_MV, PWM_TOPE);
#else
    printf("Ciclo    lectura cruda tal cual: 0..%d -> 0..%d\n", ADC_MAX, ADC_MAX);
#endif
    printf("         LED de %s comun | fuerza de salida: nivel %d de 3 (registros del chip)\n",
           LED_ANODO_COMUN ? "anodo" : "catodo", (int)led.drv);
    printf("Comandos: r = prueba de ruido del ADC, i = esta cabecera\n");
    printf("----------------------------------------------------------\n");
}

// Mide n veces la entrada, promediando "grupo" lecturas en cada medida.
typedef struct { double media, desviacion, minimo, maximo; } estadistica_t;

static estadistica_t medir_entrada(int n, int grupo)
{
    double suma = 0, suma_cuadrados = 0, minimo = 1e9, maximo = -1;
    for (int i = 0; i < n; i++) {
        int acumulado = 0;
        for (int k = 0; k < grupo; k++) {
            int cruda;
            ESP_ERROR_CHECK(adc_oneshot_read(adc, canal_adc, &cruda));
            acumulado += cruda;
        }
        double v = (double)acumulado / grupo;
        suma += v;
        suma_cuadrados += v * v;
        if (v < minimo) minimo = v;
        if (v > maximo) maximo = v;
    }
    double media = suma / n;
    double varianza = suma_cuadrados / n - media * media;
    return (estadistica_t){ media, varianza > 0 ? sqrt(varianza) : 0, minimo, maximo };
}

// Prueba de ruido: con el potenciometro quieto, la lectura deberia ser
// siempre la misma; la dispersion que aparezca es ruido del ADC y del
// montaje. Se mide con lecturas sueltas (como hace el bucle principal) y con
// promedios de 16, que es lo que recomienda la hoja de datos.
static void prueba_ruido(void)
{
    printf("--- Prueba de ruido del ADC (no mueva el potenciometro) ---\n");
    const struct { int n, grupo; const char *nombre; } casos[] = {
        { 1000, 1,  "1000 lecturas sueltas   " },
        {  200, 16, " 200 promedios de 16    " },
    };
    for (int i = 0; i < 2; i++) {
        estadistica_t e = medir_entrada(casos[i].n, casos[i].grupo);
        printf("%s media %7.1f | min %6.1f | max %6.1f | rango %5.1f | desv. tipica %5.2f cuentas\n",
               casos[i].nombre, e.media, e.minimo, e.maximo, e.maximo - e.minimo,
               e.desviacion);
    }
    printf("-----------------------------------------------------------\n");
}

// Atiende los comandos del monitor serie sin bloquear el bucle.
static void atender_comandos(void)
{
    uint8_t c;
    if (uart_read_bytes(UART_CONSOLA, &c, 1, 0) != 1) return;
    if (c == 'r' || c == 'R') prueba_ruido();
    if (c == 'i' || c == 'I') informar_configuracion();
}

/* ------------------------------------------------------------ app_main -- */

void app_main(void)
{
    configurar_consola();
    configurar_adc();
    configurar_pwm();
    informar_configuracion();

    int64_t ultimo_informe_us = 0;

    while (1) {
        // 1) Leer la entrada analogica: lectura cruda (0..~3433 en el C6) y
        //    su tension calibrada, de la misma conversion.
        int cruda, milivoltios;
        leer_potenciometro(&cruda, &milivoltios);

        // 2) Calcular el ciclo de trabajo y mandarlo a la salida PWM.
        uint32_t ciclo = calcular_ciclo(cruda, milivoltios);
        escribir_brillo(ciclo);

        // 3) Informar cada PERIODO_INFORME_MS.
        int64_t ahora_us = esp_timer_get_time();
        if (ahora_us - ultimo_informe_us >= PERIODO_INFORME_MS * 1000) {
            ultimo_informe_us = ahora_us;
            printf("adc:%d mV:%d pwm_pct:%.1f\n", cruda, milivoltios,
                   ciclo * 100.0 / PWM_TOPE);
        }

        atender_comandos();

        // 4) Ceder el nucleo: app_main() se ejecuta dentro de la tarea "main"
        //    de FreeRTOS, y una tarea que no se bloquea deja sin CPU a las
        //    demas (practica de multitareas).
        vTaskDelay(pdMS_TO_TICKS(PERIODO_BUCLE_MS));
    }
}
