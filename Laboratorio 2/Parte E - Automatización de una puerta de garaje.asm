.include "m328pdef.inc"

; registros
.def tmp = r16        ; auxiliar
.def est = r17        ; estado actual
.def dat = r18        ; dato uart

; estados
.equ E_CER = 0        ; puerta cerrada
.equ E_ABR = 1        ; abriendo
.equ E_ABI = 2        ; abierta
.equ E_CRA = 3        ; cerrando
.equ E_DET = 4        ; detenido por obstaculo

; vectores de interrupcion
.org 0x0000
    rjmp inicio       ; reset 

.org 0x0003
    rjmp isr_pcint0   ; pcint0 para sensor s3


inicio:
    ;stack
    ldi tmp, low(RAMEND)
    out SPL, tmp
    ldi tmp, high(RAMEND)
    out SPH, tmp

    ; portd: pd6 y pd7 salidas motor, pd2 a pd5 entradas con pull-up
    ldi tmp, (1<<PORTD6) | (1<<PORTD7)
    out DDRD, tmp
    ldi tmp, (1<<PORTD2) | (1<<PORTD3) | (1<<PORTD4) | (1<<PORTD5)
    out PORTD, tmp

    ; portb: pb1 salida alarma, pb0 entrada obstaculo
    ldi tmp, (1<<PORTB1)
    out DDRB, tmp
    ldi tmp, (1<<PORTB0)
    out PORTB, tmp

    ; usart a 9600 baudios (16mhz)
    ldi tmp, 0
    sts UBRR0H, tmp
    ldi tmp, 103
    sts UBRR0L, tmp
    ldi tmp, (1<<TXEN0)
    sts UCSR0B, tmp
    ldi tmp, (1<<UCSZ01) | (1<<UCSZ00)
    sts UCSR0C, tmp

    ; habilitar interrupcion pcint0 en pb0
    ldi tmp, (1<<PCIE0)
    sts PCICR, tmp
    ldi tmp, (1<<PCINT0)
    sts PCMSK0, tmp

    ; estado inicial
    ldi est, E_CER
    rcall apagar_todo

    ldi ZL, low(txt_cer << 1)
    ldi ZH, high(txt_cer << 1)
    rcall tx_str

    sei

; bucle principal
loop:
    cpi est, E_CER
    breq eval_cer

    cpi est, E_ABR
    breq eval_abr

    cpi est, E_ABI
    breq eval_abi

    cpi est, E_CRA
    breq eval_cra

    cpi est, E_DET
    breq eval_det

    rjmp loop

; estado cerrada
eval_cer:
    sbis PIND, PIND2   ; boton abrir presionando
    rjmp cmd_abrir
    rjmp loop

cmd_abrir:
    ldi est, E_ABR
    sbi PORTD, PORTD6  ; encender motor abrir
    cbi PORTD, PORTD7
    sbi PORTB, PORTB1  ; encender alarma
    ldi ZL, low(txt_abr << 1)
    ldi ZH, high(txt_abr << 1)
    rcall tx_str
    rcall delay
    rjmp loop

; estado abriendo
eval_abr:
    sbis PIND, PIND4   ; fin de carrera s1
    rjmp fin_abrir
    rjmp loop

fin_abrir:
    ldi est, E_ABI
    rcall apagar_todo
    ldi ZL, low(txt_abi << 1)
    ldi ZH, high(txt_abi << 1)
    rcall tx_str
    rjmp loop

; estado abierta
eval_abi:
    sbis PIND, PIND3   ; boton cerrar presionado
    rjmp cmd_cerrar
    rjmp loop

cmd_cerrar:
    ldi est, E_CRA
    cbi PORTD, PORTD6
    sbi PORTD, PORTD7  ; encender motor cerrar
    sbi PORTB, PORTB1  ; encender alarma
    ldi ZL, low(txt_cra << 1)
    ldi ZH, high(txt_cra << 1)
    rcall tx_str
    rcall delay
    rjmp loop

; estado cerrando
eval_cra:
    sbis PIND, PIND5   ; fin de carrera s2
    rjmp fin_cerrar
    rjmp loop

fin_cerrar:
    ldi est, E_CER
    rcall apagar_todo
    ldi ZL, low(txt_cer << 1)
    ldi ZH, high(txt_cer << 1)
    rcall tx_str
    rjmp loop

; estado detenido por obstaculo
eval_det:
    sbis PIND, PIND2   ; reanudar apertura
    rjmp cmd_abrir
    sbis PIND, PIND3   ; reanudar cierre
    rjmp cmd_cerrar
    rjmp loop

; interrupcion por obstaculo (pb0)
isr_pcint0:
    push tmp
    in tmp, SREG
    push tmp
    push ZL
    push ZH

    cpi est, E_ABR
    breq hay_obstaculo
    cpi est, E_CRA
    breq hay_obstaculo
    rjmp fin_isr

hay_obstaculo:
    rcall apagar_todo
    ldi est, E_DET

    ldi ZL, low(txt_obs << 1)
    ldi ZH, high(txt_obs << 1)
    rcall tx_str

    ldi ZL, low(txt_det << 1)
    ldi ZH, high(txt_det << 1)
    rcall tx_str

fin_isr:
    pop ZH
    pop ZL
    pop tmp
    out SREG, tmp
    pop tmp
    reti

; subrutinas
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

delay:
    ldi r19, 80
d1: ldi r20, 200
d2: dec r20
    brne d2
    dec r19
    brne d1
    ret

txt_abr: .DB "Puerta abriendo.", 13, 10, 0, 0
txt_abi: .DB "Puerta abierta.", 13, 10, 0
txt_cra: .DB "Puerta cerrando.", 13, 10, 0, 0
txt_cer: .DB "Puerta cerrada.", 13, 10, 0
txt_obs: .DB "Obstaculo detectado.", 13, 10, 0, 0
txt_det: .DB "Movimiento detenido por seguridad.", 13, 10, 0, 0
