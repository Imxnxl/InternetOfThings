# Proyecto ESP-IDF: suscriptor MQTT (canal de ThingSpeak) - ESP32-C6
# Estas lineas tienen que ir en este orden para que CMake funcione.
cmake_minimum_required(VERSION 3.22)

# Linea indicada en la practica: anade el componente de los ejemplos de
# ESP-IDF que conecta el Wi-Fi con example_connect().
set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/common_components/protocol_examples_common)
# Componente propio: red Wi-Fi guardada en la placa y cambiable por USB
# desde tablero.html (firmware/wifi_red). Se comparte con el otro programa.
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../wifi_red")
# Componente propio: LED RGB que muestra con un color la temperatura recibida
# por MQTT (firmware/led_temperatura). Tambien se comparte.
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../led_temperatura")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# Compila solo main y los componentes que usa (como los ejemplos de ESP-IDF 6).
idf_build_set_property(MINIMAL_BUILD ON)
project(mqtt_suscriptor)
