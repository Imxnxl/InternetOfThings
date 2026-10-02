# Proyecto ESP-IDF: potenciometro (ADC) -> brillo de un LED (PWM) en el ESP32-C6.
#
#   idf.py set-target esp32c6     (solo la primera vez)
#   idf.py build
#   idf.py -p COM5 flash monitor
cmake_minimum_required(VERSION 3.22)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# Compilar solo los componentes que pide main/ (mas rapido que el sistema completo).
idf_build_set_property(MINIMAL_BUILD ON)
project(potenciometro_pwm)
