# Componente compartido por el publicador y el suscriptor: red Wi-Fi guardada
# en NVS y cambiable por USB desde tablero.html (ver include/wifi_red.h).
idf_component_register(SRCS "wifi_red.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES esp_wifi esp_netif esp_event nvs_flash
                                     esp_driver_uart vfs)
