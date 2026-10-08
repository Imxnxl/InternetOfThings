# El componente espressif/mqtt no se nombra aqui: lo anade idf_component.yml
# (idf.py add-dependency espressif/mqtt).
idf_component_register(SRCS "main.c"
                       PRIV_REQUIRES nvs_flash esp_wifi esp_netif esp_event
                                     esp_driver_tsens protocol_examples_common wifi_red led_temperatura
                       # firmware/credenciales.h, compartido con el suscriptor
                       PRIV_INCLUDE_DIRS "../..")
