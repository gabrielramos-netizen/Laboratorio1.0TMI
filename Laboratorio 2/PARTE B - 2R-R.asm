;Parte B - 2R-R, señales 5 y 6


.include "m328pdef.inc"

;r16   registro temporal 
;r19   5 = Triangular (Bucle), 6 = (One-Shot)
;r21 Contador de muestras enviadas (0 a 255)
;r22 Flag de reproducción (1 = Activo, 0 = Detenido)

.equ VAL_UBRR = 103         ; 9600 Baudios a 16MHz

; VECTORES DE INTERRUPCIÓN
.org 0x0000
    rjmp REINICIO
.org OC1Aaddr               ; Vector Timer1 Compare Match A
    rjmp ISR_TIMER1_COMPA

; INICIALIZACIÓN
REINICIO:
    cli                     ; Desactivar interrupciones

    ; Configurar Stack Pointer
    ldi r16, HIGH(RAMEND)
    out SPH, r16
    ldi r16, LOW(RAMEND)
    out SPL, r16

    ; Configurar Puertos B (PB0-PB5) y C (PC0-PC1) como salidas DAC
    ldi r16, 0x3F           ; PB0-PB5 como salidas (Bits 0 a 5)
    out DDRB, r16
    clr r16
    out PORTB, r16

    ldi r16, 0x03           ; PC0-PC1 como salidas (Bits 6 y 7)
    out DDRC, r16
    clr r16
    out PORTC, r16

    ; Activar Pull-Up en RX (PD0)
    sbi PORTD, PORTD0

    ; Configurar USART (9600 Baudios, 8N1)
    ldi r16, HIGH(VAL_UBRR)
    sts UBRR0H, r16
    ldi r16, LOW(VAL_UBRR)
    sts UBRR0L, r16
    
    ldi r16, (1<<RXEN0) | (1<<TXEN0)
    sts UCSR0B, r16
    
    ldi r16, (1<<UCSZ01) | (1<<UCSZ00)
    sts UCSR0C, r16

    ; Imprimir Menú Inicial por Serial
    rcall IMPRIMIR_MENU

    ; Configurar Timer1 CTC (Frecuencia de muestreo ~10 kHz)
    clr r16
    sts TCCR1A, r16
    
    ldi r16, HIGH(199)
    sts OCR1AH, r16
    ldi r16, LOW(199)
    sts OCR1AL, r16

    ldi r16, (1<<WGM12) | (1<<CS11)  ; Modo CTC, Prescaler 8
    sts TCCR1B, r16

    ldi r16, (1<<OCIE1A)              ; Habilita Interrupción Compare Match A
    sts TIMSK1, r16

    ; Estado Inicial: Cargar Señal 5
    ldi r19, 5
    rcall INIC_SENAL5

    sei                     ; Habilita Interrupciones Globales


; BUCLE PRINCIPAL:
PRINCIPAL:
    lds r16, UCSR0A
    sbrs r16, RXC0
    rjmp PRINCIPAL

    lds temp, UDR0

    cpi r16, '5'
    breq SELECCIONAR_5

    cpi r16, '6'
    breq SELECCIONAR_6

    rjmp PRINCIPAL

SELECCIONAR_5:
    cli
    ldi r16, 5
    rcall INIC_SENAL5
    sei
    rjmp PRINCIPAL

SELECCIONAR_6:
    cli
    ldi r16, 6
    rcall INIC_SENAL6
    sei
    rjmp PRINCIPAL

; RUTINAS AUXILIARES
INIC_SENAL5:
    ldi ZH, HIGH(TABLA_SENAL5 * 2)
    ldi ZL, LOW(TABLA_SENAL5 * 2)
    clr cont_muestras
    ldi r22, 1        ; Activa reproducción continua (Bucle)
    ret

INIC_SENAL6:
    ldi ZH, HIGH(TABLA_SENAL6 * 2)
    ldi ZL, LOW(TABLA_SENAL6 * 2)
    clr cont_muestras
    ldi r22, 1        ; Activa disparo único (One-Shot)
    ret

IMPRIMIR_MENU:
    ldi ZH, HIGH(MENSAJE_MENU * 2)
    ldi ZL, LOW(MENSAJE_MENU * 2)
BUCLE_IMPRESION:
    lpm r16, Z+
    tst r16
    breq FIN_IMPRESION
    rcall TRANSMITIR_USART
    rjmp BUCLE_IMPRESION
FIN_IMPRESION:
    ret

TRANSMITIR_USART:
    lds temp2, UCSR0A
    sbrs temp2, UDRE0
    rjmp TRANSMITIR_USART
    sts UDR0, r16
    ret


; ISR TIMER 1 - ENVÍO DE MUESTRAS DAC

ISR_TIMER1_COMPA:
    in reg_estado, SREG
    push r16
    push r17

    ; Verifica si la reproducción está activa
    tst en_ejecucion
    breq SALIDA_ISR

    ; 1. Lee dato completo de 8 bits de la Flash
    lpm r18, Z+

    ; 2. Prepara bits 0..5 para PORTB
    mov r16, r18
    andi r16, 0x3F           ; Quedan solo bits 0,1,2,3,4,5 (PB0-PB5)

    ; 3. Prepara bits 6..7 para PORTC
    mov r17, r18
    lsr r17
    lsr r17
    lsr r17
    lsr r17
    lsr r17
    lsr r17                  ; Mueve bit6 a bit0 y bit7 a bit1
    andi r17, 0x03           ; Quedan solo PC0 y PC1

    ; 4. Actualización de los puertos
    out PORTB, r16
    out PORTC, r17

    ; 5. Incrementa contador de muestras
    inc r21
    brne SALIDA_ISR

    ; 6. Fin de ciclo de 256 muestras
    cpi r19, 5
    breq REINICIAR_S5

    ; Si es Señal 6 (one shot), detener
    clr r22
    rjmp SALIDA_ISR

REINICIAR_S5:
    ldi ZH, HIGH(TABLA_SENAL5 * 2)
    ldi ZL, LOW(TABLA_SENAL5 * 2)

SALIDA_ISR:
    pop r17
    pop r16
    out SREG, r20
    reti


; TABLAS DE DATOS EN MEMORIA FLASH

MENSAJE_MENU:
    .db 0x0D, 0x0A, "=== SELECCIONE SENAL ===", 0x0D, 0x0A, "5. Senal 5 (Triangular - Bucle)", 0x0D, 0x0A, "6. Senal 6 (One Shot)", 0x0D, 0x0A, 0x00

TABLA_SENAL5:
    .db 0x00, 0x02, 0x04, 0x06, 0x08, 0x0a, 0x0c, 0x0e, 0x10, 0x12, 0x14, 0x16, 0x18, 0x1a, 0x1c, 0x1e
    .db 0x20, 0x22, 0x24, 0x26, 0x28, 0x2a, 0x2c, 0x2e, 0x30, 0x32, 0x34, 0x36, 0x38, 0x3a, 0x3c, 0x3e
    .db 0x40, 0x42, 0x44, 0x46, 0x48, 0x4a, 0x4c, 0x4e, 0x50, 0x52, 0x54, 0x56, 0x58, 0x5a, 0x5c, 0x5e
    .db 0x60, 0x62, 0x64, 0x66, 0x68, 0x6a, 0x6c, 0x6e, 0x70, 0x72, 0x74, 0x76, 0x78, 0x7a, 0x7c, 0x7e
    .db 0x80, 0x82, 0x84, 0x86, 0x88, 0x8a, 0x8c, 0x8e, 0x90, 0x92, 0x94, 0x96, 0x98, 0x9a, 0x9c, 0x9e
    .db 0xa0, 0xa2, 0xa4, 0xa6, 0xa8, 0xaa, 0xac, 0xae, 0xb0, 0xb2, 0xb4, 0xb6, 0xb8, 0xba, 0xbc, 0xbe
    .db 0xc0, 0xc2, 0xc4, 0xc6, 0xc8, 0xca, 0xcc, 0xce, 0xd0, 0xd2, 0xd4, 0xd6, 0xd8, 0xda, 0xdc, 0xde
    .db 0xe0, 0xe2, 0xe4, 0xe6, 0xe8, 0xea, 0xec, 0xee, 0xf0, 0xf2, 0xf4, 0xf6, 0xf8, 0xfa, 0xfc, 0xfe
    .db 0xfe, 0xfc, 0xfa, 0xf8, 0xf6, 0xf4, 0xf2, 0xf0, 0xee, 0xec, 0xea, 0xe8, 0xe6, 0xe4, 0xe2, 0xe0
    .db 0xde, 0xdc, 0xda, 0xd8, 0xd6, 0xd4, 0xd2, 0xd0, 0xce, 0xcc, 0xca, 0xc8, 0xc6, 0xc4, 0xc2, 0xc0
    .db 0xbe, 0xbc, 0xba, 0xb8, 0xb6, 0xb4, 0xb2, 0xb0, 0xae, 0xac, 0xaa, 0xa8, 0xa6, 0xa4, 0xa2, 0xa0
    .db 0x9e, 0x9c, 0x9a, 0x98, 0x96, 0x94, 0x92, 0x90, 0x8e, 0x8c, 0x8a, 0x88, 0x86, 0x84, 0x82, 0x80
    .db 0x7e, 0x7c, 0x7a, 0x78, 0x76, 0x74, 0x72, 0x70, 0x6e, 0x6c, 0x6a, 0x68, 0x66, 0x64, 0x62, 0x60
    .db 0x5e, 0x5c, 0x5a, 0x58, 0x56, 0x54, 0x52, 0x50, 0x4e, 0x4c, 0x4a, 0x48, 0x46, 0x44, 0x42, 0x40
    .db 0x3e, 0x3c, 0x3a, 0x38, 0x36, 0x34, 0x32, 0x30, 0x2e, 0x2c, 0x2a, 0x28, 0x26, 0x24, 0x22, 0x20
    .db 0x1e, 0x1c, 0x1a, 0x18, 0x16, 0x14, 0x12, 0x10, 0x0e, 0x0c, 0x0a, 0x08, 0x06, 0x04, 0x02, 0x00

TABLA_SENAL6:
    .db 73, 74, 75, 75, 74, 73, 73, 73, 73, 72, 71, 69, 68, 67, 67, 67
    .db 68, 68, 67, 65, 62, 61, 59, 57, 56, 55, 55, 54, 54, 54, 55, 55
    .db 55, 55, 55, 55, 54, 53, 51, 50, 49, 49, 52, 61, 77, 101, 132, 169
    .db 207, 238, 255, 254, 234, 198, 154, 109, 68, 37, 17, 5, 0, 1, 6, 13
    .db 20, 28, 36, 45, 52, 57, 61, 64, 65, 66, 67, 68, 68, 69, 70, 71
    .db 71, 71, 71, 71, 71, 71, 71, 72, 72, 72, 73, 73, 74, 75, 75, 76
    .db 77, 78, 79, 80, 81, 82, 83, 84, 86, 88, 91, 93, 96, 98, 100, 102
    .db 104, 107, 109, 112, 115, 118, 121, 123, 125, 126, 127, 127, 127, 127, 127, 126
    .db 125, 124, 121, 119, 116, 113, 109, 105, 102, 98, 95, 92, 89, 87, 84, 81
    .db 79, 77, 76, 75, 74, 73, 72, 70, 69, 68, 67, 67, 67, 68, 68, 68
    .db 69, 69, 69, 69, 69, 69, 69, 70, 71, 72, 73, 73, 74, 74, 75, 75
    .db 75, 75, 75, 75, 74, 74, 73, 73, 73, 73, 72, 72, 72, 71, 71, 71
    .db 71, 71, 71, 71, 70, 70, 70, 69, 69, 69, 69, 69, 70, 70, 70, 69
    .db 68, 68, 67, 67, 67, 67, 66, 66, 66, 65, 65, 65, 65, 65, 65, 65
    .db 65, 64, 64, 63, 63, 64, 64, 65, 65, 65, 65, 65, 65, 65, 64, 64
    .db 64, 64, 64, 64, 64, 64, 65, 65, 65, 66, 67, 68, 69, 71, 72, 73