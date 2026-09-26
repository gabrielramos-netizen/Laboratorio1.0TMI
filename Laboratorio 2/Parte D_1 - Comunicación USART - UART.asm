;=====TRANSMISOR=====

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

    ldi r16, (1<<UCSZ01)|(1<<UCSZ00)
    sts UCSR0C, r16

loop:
    in r16, PINC   
    com r16           
    andi r16, 0x07    
    rcall enviar
    rjmp loop

enviar:
    lds r17, UCSR0A
    sbrs r17, UDRE0
    rjmp enviar
    sts UDR0, r16
    ret
