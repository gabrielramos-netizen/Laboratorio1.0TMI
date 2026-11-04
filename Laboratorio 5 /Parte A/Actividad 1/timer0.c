#include "timer0.h"

static volatile uint32_t ms_ticks = 0;

/* ISR ejecutada cada 1 ms usando el modo CTC del Timer0 */
ISR(TIMER0_COMPA_vect) {
    ms_ticks++;
}

void timer0_init(void) {
    /* Configurar Timer0 en modo CTC (Clear Timer on Compare Match) */
    TCCR0A = (1 << WGM01);
    
    /* Configurar Prescaler = 64 (CS01 = 1, CS00 = 1) */
    TCCR0B = (1 << CS01) | (1 << CS00);
    
    /* Valor de comparación para 1 ms con F_CPU = 16 MHz: (16000000 / (64 * 1000)) - 1 = 249 */
    OCR0A = 249;
    
    /* Habilitar la interrupción por comparación del Timer0 */
    TIMSK0 |= (1 << OCIE0A);
}

uint32_t millis(void) {
    uint32_t ms;
    cli(); /* Lectura atómica de la variable de 32 bits */
    ms = ms_ticks;
    sei();
    return ms;
}
