#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#include "SPI.h"

#define CMD_ESTADO    0x01
#define CMD_VELOCIDAD 0x02
#define CMD_PISTON    0x03
#define CMD_EMERG     0x04

volatile uint8_t cmd_recibido = 0;
volatile uint8_t val_recibido = 0;
volatile uint8_t byte_estado = 0;
volatile uint8_t flag_comando = 0;

uint8_t estado_sistema = 0;

// Interrupción para recepción de datos por SPI
ISR(SPI_STC_vect) {
    uint8_t dato = SPDR;
    
    if (byte_estado == 0) {
        cmd_recibido = dato;
        byte_estado = 1;
    } else {
        val_recibido = dato;
        byte_estado = 0;
        flag_comando = 1;
    }
}

// Inicialización del módulo SPI en modo esclavo
void spi_esclavo_init(void) {
    DDRB |= (1 << PB4); // MISO como salida
    DDRB &= ~((1 << PB2) | (1 << PB3) | (1 << PB5));
    SPCR = (1 << SPE) | (1 << SPIE);
}

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

// Rutina de movimiento del pistón y activación del buzzer
void accionar_piston(void) {
    PORTD |= (1 << PD3);
    _delay_ms(50);
    PORTD &= ~(1 << PD3);
    
    OCR1A = 3200; // Desplazamiento del servo
    _delay_ms(300);
    OCR1A = 2000; // Retorno a posición de reposo
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
    spi_esclavo_init();

    sei(); // Habilitación de interrupciones globales

    while (1) {
        if (flag_comando) {
            flag_comando = 0;
            
            switch (cmd_recibido) {
                case CMD_ESTADO:
                    if (val_recibido == 1) {
                        estado_sistema = 1;
                        PORTD |= (1 << PD7);  // LED verde
                        PORTD &= ~(1 << PD4);
                    } else {
                        estado_sistema = 0;
                        PORTD &= ~(1 << PD7);
                        PORTD |= (1 << PD4);  // LED rojo
                        OCR0A = 0;
                    }
                    break;
                    
                case CMD_VELOCIDAD:
                    if (estado_sistema == 1) {
                        OCR0A = val_recibido; // Actualización de velocidad del motor
                    }
                    break;
                    
                case CMD_PISTON:
                    if (estado_sistema == 1) {
                        accionar_piston();
                    }
                    break;
                    
                case CMD_EMERG:
                    estado_sistema = 2;
                    OCR0A = 0;
                    PORTD &= ~(1 << PD7);
                    break;
            }
        }
        
        // Alarma visual y sonora en estado de emergencia
        if (estado_sistema == 2) {
            PORTD ^= (1 << PD4);
            PORTD |= (1 << PD3);
            _delay_ms(150);
            PORTD &= ~(1 << PD3);
            _delay_ms(150);
        }
    }
}
