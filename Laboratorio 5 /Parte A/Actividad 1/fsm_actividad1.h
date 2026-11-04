#ifndef FSM_ACTIVIDAD1_H_
#define FSM_ACTIVIDAD1_H_

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include "timer0.h"

/* Definición del tipo puntero a función para implementar la FSM (Forma 2 de clase) */
typedef void (*EstadoFuncion_t)(void);

void fsm_init(void);
void fsm_run(void);
void fsm_registrar_actividad(void);

#endif /* FSM_ACTIVIDAD1_H_ */
