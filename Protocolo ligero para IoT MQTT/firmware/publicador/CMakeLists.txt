# Proyecto ESP-IDF: publicador MQTT (temperatura -> ThingSpeak) - ESP32-C6
# Estas lineas tienen que ir en este orden para que CMake funcione.
cmake_minimum_required(VERSION 3.22)

# Linea indicada en la practica: anade el componente de los ejemplos de
# ESP-IDF que conecta el Wi-Fi con example_connect().
set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/common_components/protocol_examples_common)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# Compila solo main y los componentes que usa (como los ejemplos de ESP-IDF 6).
idf_build_set_property(MINIMAL_BUILD ON)
project(mqtt_publicador)
