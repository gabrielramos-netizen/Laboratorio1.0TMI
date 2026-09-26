.include "m328pdef.inc"

.equ ubrr = 103

.org 0x0000
    rjmp inicio

inicio:
    ; pila
    ldi r16, HIGH(RAMEND)
    out SPH, r16
    ldi r16, LOW(RAMEND)
    out SPL, r16

    ; leds salida (PB0..PB5 y PC0..PC1)
    ldi r16, 0x3F
    out DDRB, r16
    ldi r16, 0x03
    out DDRC, r16

    ; leds apagaos
    clr r16
    out PORTB, r16
    out PORTC, r16

    ; config usart rx
    ldi r16, 0
    sts UBRR0H, r16
    ldi r16, ubrr
    sts UBRR0L, r16

    ldi r16, (1<<RXEN0)
    sts UCSR0B, r16

    ldi r16, (1<<UCSZ01)|(1<<UCSZ00)
    sts UCSR0C, r16

loop:
    rcall recibir
    andi r16, 0x07
    rjmp loop

recibir:
    lds r17, UCSR0A
    sbrs r17, RXC0
    rjmp recibir
    lds r16, UDR0
    ret
