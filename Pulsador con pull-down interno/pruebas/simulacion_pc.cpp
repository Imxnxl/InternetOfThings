/*
 * simulacion_pc.cpp - Prueba de la logica del sketch en el PC
 * ============================================================================
 * Compila EL MISMO pulsador_pulldown_interno.ino (se incluye tal cual, sin
 * copiarlo) contra un Arduino simulado, le aplica una entrada guionizada y
 * comprueba automaticamente:
 *
 *   1) que en cada vuelta de loop() el LED muestra lo que se acaba de leer
 *      en la entrada (verde encendido con HIGH, todo apagado con LOW);
 *   2) que el retardo entre un cambio de la entrada y el cambio del LED no
 *      supera un periodo de sondeo (1 ms);
 *   3) que el registro serie informa exactamente de las pulsaciones del
 *      guion, con sus rebotes contados, y descarta el toque demasiado corto.
 *
 * Que NO prueba: la electronica. El pull-down, los niveles de tension y el
 * LED fisico solo se pueden comprobar en la placa (ver el informe).
 *
 * El tiempo se simula en microsegundos. Los cambios del guion caen a 300 us
 * de cada milisegundo para que no coincidan con el instante de lectura, como
 * pasaria con un pulsador real.
 */
#include "Arduino.h"
#include "driver/gpio.h"

#include <map>
#include <string>
#include <vector>

#include "../firmware/pulsador_pulldown_interno/pulsador_pulldown_interno.ino"

/* ------------------------------------------------------------ Guion -- */
struct Flanco { uint32_t t_us; int nivel; };

static uint32_t ms(uint32_t m) { return m * 1000 + 300; }

static const std::vector<Flanco> GUION = {
    // A. Pulsacion limpia de 400 ms
    { ms(2000), HIGH }, { ms(2400), LOW },
    // B. Pulsacion con rebotes: 5 cambios al cerrar y 3 al abrir
    { ms(4000), HIGH }, { ms(4001), LOW }, { ms(4002), HIGH }, { ms(4004), LOW }, { ms(4005), HIGH },
    { ms(4600), LOW },  { ms(4601), HIGH }, { ms(4603), LOW },
    // C. Toque de 10 ms: mas corto que el antirrebote (30 ms), debe descartarse
    { ms(6000), HIGH }, { ms(6010), LOW },
    // D. Pulsacion larga de 2 s
    { ms(7000), HIGH }, { ms(9000), LOW },
};
static const uint32_t FIN_US = 10000u * 1000u;

// Lineas que el registro serie debe producir, en este orden.
static const std::vector<std::vector<std::string>> ESPERADO = {
    { "[    2001 ms] PRESIONADO", "pulsacion #1", "cambios leidos: 1" },
    { "[    2401 ms] SUELTO",     "duracion: 400 ms", "cambios leidos: 1" },
    { "[    4001 ms] PRESIONADO", "pulsacion #2", "cambios leidos: 5" },
    { "[    4601 ms] SUELTO",     "duracion: 600 ms", "cambios leidos: 3" },
    { "[    6001 ms] pulso de menos de 30 ms", "cambios leidos: 2" },
    { "[    7001 ms] PRESIONADO", "pulsacion #3", "cambios leidos: 1" },
    { "[    9001 ms] SUELTO",     "duracion: 2000 ms", "cambios leidos: 1" },
};

/* --------------------------------------------------- Arduino simulado -- */
SerialSimulado Serial;
EspSimulado    ESP;

static uint32_t t_us = 0;
static std::map<uint8_t, uint8_t> modo, salida;
static std::map<uint8_t, gpio_drive_cap_t> fuerza;
static int ultimaEntradaLeida = LOW;
static std::vector<std::pair<uint32_t, int>> cambiosLedVerde;   // (t_us, nivel)

static int nivelBoton(uint32_t t)
{
    int n = LOW;
    for (const Flanco &f : GUION)
        if (f.t_us <= t) n = f.nivel;
    return n;
}

uint32_t millis()             { return t_us / 1000; }
void     delay(uint32_t m)    { t_us += m * 1000; }
uint32_t getCpuFrequencyMhz() { return 160; }

void pinMode(uint8_t pin, uint8_t m)
{
    modo[pin] = m;
    if (!fuerza.count(pin)) fuerza[pin] = GPIO_DRIVE_CAP_DEFAULT;
    salida[pin] = LOW;
}

void digitalWrite(uint8_t pin, uint8_t nivel)
{
    if (pin == PIN_LED_G && salida[pin] != nivel) cambiosLedVerde.push_back({ t_us, nivel });
    salida[pin] = nivel;
}

int digitalRead(uint8_t pin)
{
    if (pin == PIN_BOTON) return ultimaEntradaLeida = nivelBoton(t_us);
    return salida[pin];
}

esp_err_t gpio_set_drive_capability(gpio_num_t pin, gpio_drive_cap_t f)
{
    fuerza[pin] = f;
    return 0;
}

// Reproduce lo que haria el hardware: la configuracion sale de pinMode().
esp_err_t gpio_get_io_config(gpio_num_t pin, gpio_io_config_t *cfg)
{
    *cfg = {};
    cfg->pd  = (modo[pin] & PULLDOWN) != 0;
    cfg->ie  = (modo[pin] & INPUT) != 0;
    cfg->oe  = (modo[pin] & OUTPUT) == OUTPUT;
    cfg->drv = fuerza[pin];
    return 0;
}

/* ------------------------------------------------------------- Prueba -- */
int main()
{
    setup();
    cambiosLedVerde.clear();   // los de la prueba de arranque no cuentan

    int vueltas = 0, discrepancias = 0;
    while (t_us < FIN_US) {
        loop();
        vueltas++;
        bool debeEncender = (ultimaEntradaLeida == HIGH);
        bool verde = salida[PIN_LED_G] == HIGH;
        bool rojo  = salida[PIN_LED_R] == HIGH;
        bool azul  = salida[PIN_LED_B] == HIGH;
        if (verde != debeEncender || rojo || azul) discrepancias++;
    }

    const std::string &registro = Serial.registro;

    // Retardo entrada -> LED: para cada cambio real de la entrada, el primer
    // cambio del LED verde al mismo nivel.
    uint32_t retardoMax = 0;
    int cambiosEntrada = 0;
    int nivelPrevio = LOW;
    for (const Flanco &f : GUION) {
        if (f.nivel == nivelPrevio) continue;
        nivelPrevio = f.nivel;
        cambiosEntrada++;
        for (auto &c : cambiosLedVerde) {
            if (c.first >= f.t_us && c.second == f.nivel) {
                if (c.first - f.t_us > retardoMax) retardoMax = c.first - f.t_us;
                break;
            }
        }
    }

    // Eventos del registro, en orden.
    std::vector<std::string> eventos;
    size_t ini = 0, fin;
    while ((fin = registro.find('\n', ini)) != std::string::npos) {
        std::string linea = registro.substr(ini, fin - ini);
        if (!linea.empty() && linea[0] == '[') eventos.push_back(linea);
        ini = fin + 1;
    }
    int correctos = 0;
    for (size_t i = 0; i < ESPERADO.size() && i < eventos.size(); i++) {
        bool ok = true;
        for (const std::string &trozo : ESPERADO[i])
            if (eventos[i].find(trozo) == std::string::npos) ok = false;
        if (ok) correctos++;
        else std::printf("  EVENTO %zu DISTINTO DE LO ESPERADO: %s\n", i + 1, eventos[i].c_str());
    }

    bool okLed     = discrepancias == 0;
    bool okRetardo = retardoMax <= 1000;
    bool okEventos = correctos == (int)ESPERADO.size() && eventos.size() == ESPERADO.size();

    std::printf("\n=== Verificacion automatica ===\n");
    std::printf("Vueltas de loop() simuladas:        %d (%.1f s)\n", vueltas, FIN_US / 1e6);
    std::printf("LED == entrada en cada vuelta:      %s (%d discrepancias)\n",
                okLed ? "SI" : "NO", discrepancias);
    std::printf("Cambios de la entrada / del LED:    %d / %zu\n", cambiosEntrada,
                cambiosLedVerde.size());
    std::printf("Retardo maximo entrada -> LED:      %u us (limite: 1000 us)\n", retardoMax);
    std::printf("Eventos del registro correctos:     %d de %zu (registrados: %zu)\n",
                correctos, ESPERADO.size(), eventos.size());
    bool ok = okLed && okRetardo && okEventos;
    std::printf("RESULTADO: %s\n", ok ? "OK" : "FALLO");
    return ok ? 0 : 1;
}
