#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include "ultrasonido.h"

#define US_TRIG_PIN PB4
#define US_ECHO_PIN PB0

static uint8_t dist_cm = 250;

void ultrasonido_init(void) {
    DDRB |= (1 << US_TRIG_PIN);
    DDRB &= ~(1 << US_ECHO_PIN);
}

void ultrasonido_tarea(void) {
    uint16_t cnt = 0;
    PORTB &= ~(1 << US_TRIG_PIN);
    _delay_us(2);
    PORTB |= (1 << US_TRIG_PIN);
    _delay_us(10);
    PORTB &= ~(1 << US_TRIG_PIN);

    uint16_t timeout = 15000;
    while (!(PINB & (1 << US_ECHO_PIN))) {
        if (--timeout == 0) { dist_cm = 250; return; }
    }
    while (PINB & (1 << US_ECHO_PIN)) {
        _delay_us(1);
        if (++cnt > 12000) { dist_cm = 250; return; }
    }
    uint16_t cm = cnt / 38;
    dist_cm = (cm > 250) ? 250 : (uint8_t)cm;
}

uint8_t ultrasonido_get_cm(void) {
    return dist_cm;
}
