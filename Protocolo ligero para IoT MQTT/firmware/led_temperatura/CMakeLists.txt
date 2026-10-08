# Componente compartido por el publicador y el suscriptor: LED RGB que muestra
# con un color la temperatura recibida por MQTT (ver include/led_temperatura.h).
idf_component_register(SRCS "led_temperatura.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES esp_driver_gpio)
