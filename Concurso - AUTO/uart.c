#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include "uart.h"

#define BUF_N 32 // Potencia de 2 para máscara rápida
static volatile uint8_t rx_buf[BUF_N];
static volatile uint8_t rx_head = 0, rx_tail = 0;

ISR(USART_RX_vect) {
    uint8_t c = UDR0;
    uint8_t next = (rx_head + 1) & (BUF_N - 1);
    if (next != rx_tail) {
        rx_buf[rx_head] = c;
        rx_head = next;
    }
}

void uart_init(void) {
    UBRR0H = 0;
    UBRR0L = 103;                                   // 9600 baudios @ 16 MHz
    UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);         // Trama 8N1
}

uint8_t uart_hay(void) {
    return rx_head != rx_tail;
}

uint8_t uart_leer(void) {
    uint8_t c = rx_buf[rx_tail];
    rx_tail = (rx_tail + 1) & (BUF_N - 1);
    return c;
}

void uart_enviar(uint8_t b) {
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = b;
}
