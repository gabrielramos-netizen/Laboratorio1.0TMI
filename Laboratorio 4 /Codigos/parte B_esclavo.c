#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>

#include "SPI.h"

// Configuración de Timer0 para PWM de motor (PD6)
void timer0_init(void) {
    DDRD |= (1 << PD6);
    TCCR0A = (1 << WGM00) | (1 << WGM01) | (1 << COM0A1);
    TCCR0B = (1 << CS01) | (1 << CS00);
    OCR0A = 0;
}

// Configuración de Timer1 para servo del pistón (PB1)
void timer1_init(void) {
    DDRB |= (1 << PB1);
    TCCR1A = (1 << WGM11) | (1 << COM1A1);
    TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS11);
    ICR1 = 39999;
    OCR1A = 2000; // Reposo
}

int main(void) {
    // Configuración de LEDs (PD7 verde, PD4 rojo) y Buzzer (PD3)
    DDRD |= (1 << PD7) | (1 << PD4) | (1 << PD3);

    // Estado inicial: PARO
    PORTD &= ~(1 << PD7);
    PORTD |= (1 << PD4);
    PORTD &= ~(1 << PD3);

    timer0_init();
    timer1_init();

    while (1) {
        _delay_ms(100);
    }
}
