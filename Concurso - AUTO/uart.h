#ifndef UART_H
#define UART_H

#include <stdint.h>

void uart_init(void);
uint8_t uart_hay(void);
uint8_t uart_leer(void);
void uart_enviar(uint8_t b);

#endif
