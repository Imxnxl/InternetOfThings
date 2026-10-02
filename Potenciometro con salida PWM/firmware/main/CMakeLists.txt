idf_component_register(SRCS "main.c"
                       PRIV_REQUIRES esp_adc          # ADC: lectura y calibracion
                                     esp_driver_ledc  # PWM
                                     esp_driver_gpio  # pines del LED
                                     esp_driver_uart  # comandos por el monitor serie
                                     vfs              # printf a traves del driver de la UART
                                     esp_timer        # tiempo en microsegundos
                       INCLUDE_DIRS "")
