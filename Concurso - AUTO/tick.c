#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include "tick.h"

static volatile uint32_t ms_contador = 0;

ISR(TIMER2_COMPA_vect) {
    ms_contador++;
}

void tick_init(void) {
    TCCR2A = (1 << WGM21);              // Modo CTC
    TCCR2B = (1 << CS22);               // Prescaler N = 64 (1 tick = 4 us)
    OCR2A  = 249;                       // 250 * 4 us = 1000 us (1 ms exacto)
    TIMSK2 = (1 << OCIE2A);             // Habilitar interrupción por comparación
}

uint32_t millis(void) {
    uint32_t m;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { // Lectura atómica de 32 bits
        m = ms_contador;
    }
    return m;
}
