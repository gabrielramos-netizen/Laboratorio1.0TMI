;Esta es la parte A - Matriz de leds 

.include "m328pdef.inc" 
.org 0x0000
   rjmp inicio 
.org 0x0024; Vector de Recepción Completa USART (USART_RX), obtenido del datasheet del Atmega328p
   rjmp rx_interrupcion 

 ;Inicializamos la pila para las subrutinas 
ldi r16, HIGH(RAMEND) 
out SPH, r16
ldi r16, LOW(RAMEND)
out SPL, r16 

; configuramos UART a 9600 baudios 
ldi r16, 0x00
sts UBRR0H, r16 ; parte alta del registro UBRR0
ldi r16, 103 
sts UBRR0L, r16 ; parte baja del registro UBRR0

;Habilitamos la transmisión (TX), recepción (RX) e Interrupción por RX
LDI R16, (1<<RXEN0) | (1<<TXEN0) | (1<<RXCIE0)
STS UCSR0B, R16

;8 bits de datos, 1 bit de parada
 LDI R16, (1<<UCSZ01) | (1<<UCSZ00) ;definimos el tamaño de los datos
 STS UCSR0C, R16