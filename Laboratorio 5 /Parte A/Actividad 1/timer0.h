#ifndef TIMER0_H_
#define TIMER0_H_

#include <avr/io.h>
#include <avr/interrupt.h>

/* Inicializa el Timer0 para generar una base de tiempo de 1 ms */
void timer0_init(void);

/* Retorna el tiempo transcurrido en milisegundos desde el inicio */
uint32_t millis(void);

#endif /* TIMER0_H_ */
