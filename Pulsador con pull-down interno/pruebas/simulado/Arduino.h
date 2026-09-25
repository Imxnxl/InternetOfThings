/*
 * Arduino.h (simulado) - Lo minimo del core Arduino-ESP32 para compilar el
 * sketch en el PC y probar su logica.
 *
 * No simula la electronica: la entrada del pulsador la dicta un guion de
 * niveles en el tiempo (ver simulacion_pc.cpp). Lo que se prueba es el
 * programa: que el LED copie la entrada y que el registro serie informe bien
 * de pulsaciones, rebotes y pulsos cortos.
 */
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <string>

#define HIGH 1
#define LOW  0

#define INPUT          0x01
#define OUTPUT         0x03
#define PULLDOWN       0x08
#define INPUT_PULLDOWN 0x09

#define ESP_ARDUINO_VERSION_MAJOR 3
#define ESP_ARDUINO_VERSION_MINOR 3
#define ESP_ARDUINO_VERSION_PATCH 11

// Implementadas en simulacion_pc.cpp
uint32_t millis();
void     delay(uint32_t ms);
void     pinMode(uint8_t pin, uint8_t modo);
void     digitalWrite(uint8_t pin, uint8_t nivel);
int      digitalRead(uint8_t pin);
uint32_t getCpuFrequencyMhz();

// Muestra la salida por consola y guarda una copia en "registro" para que la
// prueba pueda comprobarla despues.
struct SerialSimulado {
    std::string registro;

    void begin(unsigned long) {}
    explicit operator bool() const { return true; }
    int  available() { return 0; }
    int  read() { return -1; }
    void print(const char *s) { std::fputs(s, stdout); registro += s; }
    void println(const char *s = "") { print(s); print("\n"); }
    void printf(const char *fmt, ...) __attribute__((format(printf, 2, 3)))
    {
        char linea[512];
        va_list a;
        va_start(a, fmt);
        std::vsnprintf(linea, sizeof(linea), fmt, a);
        va_end(a);
        print(linea);
    }
};
extern SerialSimulado Serial;

struct EspSimulado {
    const char *getChipModel() { return "ESP32-C6 (simulado en PC)"; }
    uint8_t     getChipRevision() { return 0; }
};
extern EspSimulado ESP;
