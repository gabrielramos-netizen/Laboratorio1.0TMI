.include "m328pdef.inc"

.def tmp = r16        ; auxiliar
.def est = r17        ; estado actual
.def dat = r18        ; dato uart

.equ E_CER = 0        ; puerta cerrada
.equ E_ABR = 1        ; abriendo
.equ E_ABI = 2        ; abierta
.equ E_CRA = 3        ; cerrando

.org 0x0000
    rjmp inicio

.org 0x0006
    rjmp isr_pcint0

inicio:
    ;stack
    ldi tmp, low(RAMEND)
    out SPL, tmp
    ldi tmp, high(RAMEND)
    out SPH, tmp

    ; configuracion de puertos
    ldi tmp, (1<<DDD6) | (1<<DDD7)
    out DDRD, tmp
    ldi tmp, (1<<PORTD2) | (1<<PORTD3) | (1<<PORTD4) | (1<<PORTD5)
    out PORTD, tmp

    ldi tmp, (1<<DDB1)
    out DDRB, tmp
    ldi tmp, (1<<PORTB0)
    out PORTB, tmp

    ; usart a 9600 baudios
    ldi tmp, 0
    sts UBRR0H, tmp
    ldi tmp, 103
    sts UBRR0L, tmp
    ldi tmp, (1<<TXEN0)
    sts UCSR0B, tmp
    ldi tmp, (1<<UCSZ01) | (1<<UCSZ00)
    sts UCSR0C, tmp

    ; pcint0 en pb0
    ldi tmp, (1<<PCIE0)
    sts PCICR, tmp
    ldi tmp, (1<<PCINT0)
    sts PCMSK0, tmp

    ldi est, E_CER
    rcall apagar_todo

    ldi ZL, low(txt_cer)
    ldi ZH, high(txt_cer)
    rcall tx_str

loop:
    cpi est, E_CER
    breq eval_cer

    cpi est, E_ABR
    breq eval_abr

    cpi est, E_ABI
    breq eval_abi

    cpi est, E_CRA
    breq eval_cra

    rjmp loop

eval_cer:
    sbis PIND, PIND2
    rjmp cmd_abrir
    rjmp loop

cmd_abrir:
    ldi est, E_ABR
    sbi PORTD, PORTD6
    sbi PORTB, PORTB1
    rjmp loop

eval_abr:
    sbis PIND, PIND4
    rjmp fin_abrir
    rjmp loop

fin_abrir:
    ldi est, E_ABI
    rcall apagar_todo
    rjmp loop

eval_abi:
    sbis PIND, PIND3
    rjmp cmd_cerrar
    rjmp loop

cmd_cerrar:
    ldi est, E_CRA
    sbi PORTD, PORTD7
    sbi PORTB, PORTB1
    rjmp loop

eval_cra:
    sbis PIND, PIND5
    rjmp fin_cerrar
    rjmp loop

fin_cerrar:
    ldi est, E_CER
    rcall apagar_todo
    rjmp loop

isr_pcint0:
    reti

apagar_todo:
    cbi PORTD, PORTD6
    cbi PORTD, PORTD7
    cbi PORTB, PORTB1
    ret

tx_str:
    lpm dat, Z+
    cpi dat, 0
    breq fin_tx
wait_tx:
    lds tmp, UCSR0A
    sbrs tmp, UDRE0
    rjmp wait_tx
    sts UDR0, dat
    rjmp tx_str
fin_tx:
    ret

txt_abr: .DB "Puerta abriendo.", 13, 10, 0
txt_abi: .DB "Puerta abierta.", 13, 10, 0
txt_cra: .DB "Puerta cerrando.", 13, 10, 0
txt_cer: .DB "Puerta cerrada.", 13, 10, 0