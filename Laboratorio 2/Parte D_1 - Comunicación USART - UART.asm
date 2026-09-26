.include "m328pdef.inc"

.equ ubrr = 103 ; 16MHz y 9600 baudios

.org 0x0000
    rjmp inicio

inicio:
    ; pila
    ldi r16, HIGH(RAMEND)
    out SPH, r16
    ldi r16, LOW(RAMEND)
    out SPL, r16

    ; entradas dip switch (PC0..PC2)
    cbi DDRC, 0
    cbi DDRC, 1
    cbi DDRC, 2
    ; pull-ups
    sbi PORTC, 0
    sbi PORTC, 1
    sbi PORTC, 2

    ; config usart tx
    ldi r16, 0
    sts UBRR0H, r16
    ldi r16, ubrr
    sts UBRR0L, r16

    ldi r16, (1<<TXEN0)
    sts UCSR0B, r16

    ldi r16, (1<<UCSZ01)|(1<<UCSZ00) ; 8 bits
    sts UCSR0C, r16

loop:
    rjmp loop