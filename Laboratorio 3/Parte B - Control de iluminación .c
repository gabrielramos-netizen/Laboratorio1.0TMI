#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Pines para el Puente H (Motor)
#define IN1 PD2
#define IN2 PD3

// Variable global para la luz deseada enviada por UART
volatile uint16_t setpoint = 500;
char buffer_rx[10];
volatile uint8_t idx = 0;

// Inicialización de UART a 9600 baudios
void uart_init(void) {
    uint16_t ubrr = 103;
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)(ubrr);
    UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0); // Habilitar RX, TX e interrupción
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void uart_send_char(char c) {
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = c;
}

void uart_send_string(char *str) {
    while (*str) {
        uart_send_char(*str++);
    }
}

// Interrupción por recepción de datos serie
#if defined(USART_RX_vect)
ISR(USART_RX_vect)
#else
ISR(USART0_RX_vect)
#endif
{
    char c = UDR0;
    // Si llega Enter, guardar el setpoint recibido
    if (c == '\n' || c == '\r') {
        buffer_rx[idx] = '\0';
        if (idx > 0) {
            uint16_t nuevo_sp = (uint16_t)atoi(buffer_rx);
            if (nuevo_sp <= 1023) {
                setpoint = nuevo_sp;
            }
        }
        idx = 0;
    } else if (idx < sizeof(buffer_rx) - 1) {
        if (c >= '0' && c <= '9') {
            buffer_rx[idx++] = c;
        }
    }
}

// Configuración del ADC
void adc_init(void) {
    ADMUX = (1 << REFS0); // Referencia AVcc (5V)
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // Prescaler en 128
}

// Lectura de canal analógico
uint16_t adc_read(uint8_t ch) {
    ADMUX = (ADMUX & 0xF0) | (ch & 0x0F);
    ADCSRA |= (1 << ADSC); // Iniciar conversión
    while (ADCSRA & (1 << ADSC)); // Esperar fin de conversión
    return ADC;
}

// Configuración de PWM en PB1 (Timer1)
void pwm_init(void) {
    DDRB |= (1 << PB1); // PB1 salida PWM
    
    // Fast PWM de 8 bits, prescaler en 64
    TCCR1A = (1 << COM1A1) | (1 << WGM10);
    TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);
    
    OCR1A = 0; // Velocidad inicial en 0
}

int main(void) {
    // Configuración de salidas del motor
    DDRD |= (1 << IN1) | (1 << IN2);
    PORTD &= ~((1 << IN1) | (1 << IN2));

    adc_init();
    pwm_init();
    uart_init();
    sei(); // Habilitar interrupciones globales

    uint16_t ldr = 0;
    uint16_t pot = 0;

    while (1) {
        ldr = adc_read(0); // LDR en A0
        pot = adc_read(1); // Potenciómetro en A1

        // Giro de prueba inicial
        PORTD |= (1 << IN1);
        PORTD &= ~(1 << IN2);
        OCR1A = 120;

        _delay_ms(100);
    }

    return 0;
}
