/*
 * ejemplos.h - Punto de entrada de cada uno de los cuatro experimentos.
 *
 * Cada funcion crea sus tareas con xTaskCreate() y retorna inmediatamente.
 * A partir de ese momento manda el planificador de FreeRTOS.
 */
#ifndef EJEMPLOS_H
#define EJEMPLOS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Ejemplo 1 - Time-slicing: dos tareas de IGUAL prioridad se reparten el
 * unico nucleo por turnos. Es el primer ejemplo del material. */
void ej1_arrancar(void);

/* Ejemplo 2 - Cola productor/consumidor: el consumidor duerme a 0% de CPU
 * hasta que llega un dato. Es el segundo ejemplo del material. */
void ej2_arrancar(void);

/* Ejemplo 3 - Starvation y Watchdog: una tarea de prioridad alta monopoliza
 * la CPU sin bloquearse y mata de hambre a las de prioridad menor. */
void ej3_arrancar(void);

/* Ejemplo 4 - Preemcion por prioridad: una tarea de prioridad alta expulsa
 * a una de prioridad baja en cuanto pasa a estado Ready. */
void ej4_arrancar(void);

/* Ejemplo 5 - Time-slicing puro: dos tareas de igual prioridad que NUNCA se
 * bloquean, para verificar el reparto por turnos que describe el material. */
void ej5_arrancar(void);

#ifdef __cplusplus
}
#endif

#endif /* EJEMPLOS_H */
