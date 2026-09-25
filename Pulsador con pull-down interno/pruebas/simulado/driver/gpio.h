/*
 * driver/gpio.h (simulado) - Las piezas de la API de ESP-IDF que usa el
 * sketch, con los mismos nombres y tipos que en ESP-IDF 5.5.
 */
#pragma once
#include <cstdint>

typedef int gpio_num_t;

typedef enum {
    GPIO_DRIVE_CAP_0 = 0,
    GPIO_DRIVE_CAP_1 = 1,
    GPIO_DRIVE_CAP_2 = 2,
    GPIO_DRIVE_CAP_DEFAULT = 2,
    GPIO_DRIVE_CAP_3 = 3,
} gpio_drive_cap_t;

typedef struct {
    uint32_t fun_sel;
    uint32_t sig_out;
    gpio_drive_cap_t drv;
    bool pu, pd, ie, oe, oe_ctrl_by_periph, oe_inv, od, slp_sel;
} gpio_io_config_t;

typedef int esp_err_t;

// Implementadas en simulacion_pc.cpp
esp_err_t gpio_set_drive_capability(gpio_num_t pin, gpio_drive_cap_t fuerza);
esp_err_t gpio_get_io_config(gpio_num_t pin, gpio_io_config_t *cfg);
