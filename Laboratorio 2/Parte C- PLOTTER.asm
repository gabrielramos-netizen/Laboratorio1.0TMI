.include "m328pbdef.inc"

; ===== DEFINICIÓN DE REGISTROS =====
.def TEMP        = r16
.def DATO_UART   = r17
.def DELAY1      = r18
.def DELAY2      = r19
.def ZONA_ACTUAL = r20   ; Contador de zona A4 (1 a 5)
.def PASOS       = r21
.def Y_POS       = r23   ; Registro para coordenada Y
.def REG_DELAY3  = r25   ; Registro auxiliar para la pausa de 1s

; ===== CONFIGURACIÓN Y CONSTANTES =====
.equ PASOS_POR_PIXEL = 10  ; Ajusta la escala del píxel
.equ PASOS_POR_MM    = 5   ; Pasos del motor por cada milímetro

.equ Z1_X = 45  
.equ Z1_Y = 45

.equ Z2_X = 145 
.equ Z2_Y = 45

.equ Z3_X = 245 
.equ Z3_Y = 45

.equ Z4_X = 95  
.equ Z4_Y = 135

.equ Z5_X = 195 
.equ Z5_Y = 135

; ===== PINES (PORTD) =====
.equ SOL_BAJAR  = (1<<2)   
.equ SOL_SUBIR  = (1<<3)   
.equ MOV_ABAJO  = (1<<4)   
.equ MOV_ARRIBA = (1<<5)   
.equ MOV_IZQ    = (1<<6)   
.equ MOV_DER    = (1<<7)   


; ===== MACROS DE DIBUJO =====

.macro IZQ
    ldi r22, @0
    rcall MOTO_IZQ_N
.endmacro

.macro DER
    ldi r22, @0
    rcall MOTO_DER_N
.endmacro

.macro ARR
    ldi r22, @0
    rcall MOTO_ARR_N
.endmacro

.macro ABJ
    ldi r22, @0
    rcall MOTO_ABJ_N
.endmacro

.macro DIAG_AD
    ldi r22, @0
    rcall MOTO_DIAG_AD_N
.endmacro

.macro DIAG_AI
    ldi r22, @0
    rcall MOTO_DIAG_AI_N
.endmacro

.macro DIAG_BD
    ldi r22, @0
    rcall MOTO_DIAG_BD_N
.endmacro

.macro DIAG_BI
    ldi r22, @0
    rcall MOTO_DIAG_BI_N
.endmacro

.macro LEVANTAR_LAPIZ
    rcall SUBIR
.endmacro

.macro BAJAR_LAPIZ
    rcall BAJAR
.endmacro

.macro ESPERAR_1S
    rcall DELAY_1SEG
.endmacro


; ===== INICIO =====

.org 0x0000
    rjmp INICIO

INICIO:
    ; Stack Pointer
    ldi TEMP, HIGH(RAMEND)
    out SPH, TEMP
    ldi TEMP, LOW(RAMEND)
    out SPL, TEMP

    ; Puerto D como Salida
    ldi TEMP, 0xFC        
    out DDRD, TEMP
    clr TEMP
    out PORTD, TEMP       

    ; Configuración USART 
    ldi TEMP, 0x00
    sts UBRR0H, TEMP
    ldi TEMP, 103         
    sts UBRR0L, TEMP

    ldi TEMP, (1<<RXEN0) | (1<<TXEN0)  ; Habilitar RX y TX
    sts UCSR0B, TEMP

    ldi TEMP, (1<<UCSZ01) | (1<<UCSZ00) ; 8 bits de datos, 1 bit de parada
    sts UCSR0C, TEMP

    ; Inicializar Zonas y Homing
    ldi ZONA_ACTUAL, 1     
    rcall HOMING

    ; Transmitir Interfaz por Serial al iniciar
    rcall MOSTRAR_MENU_SERIAL

    rjmp MAIN_LOOP         


; ===== COMUNICACIÓN SERIAL (UART) =====

UART_RECIBIR:
    lds TEMP, UCSR0A
    sbrs TEMP, RXC0       
    rjmp UART_RECIBIR     
    lds DATO_UART, UDR0  
    ret

UART_TRANSMITIR:
    lds r24, UCSR0A
    sbrs r24, UDRE0       
    rjmp UART_TRANSMITIR
    sts UDR0, TEMP
    ret

MOSTRAR_MENU_SERIAL:
    ldi ZL, LOW(TEXTO_MENU * 2)
    ldi ZH, HIGH(TEXTO_MENU * 2)
L_ENVIAR_TXT:
    lpm TEMP, Z+
    tst TEMP
    breq FIN_ENVIAR_TXT
    rcall UART_TRANSMITIR
    rjmp L_ENVIAR_TXT
FIN_ENVIAR_TXT:
    ret


; ===== MENÚ UART =====

MAIN_LOOP:
    rcall UART_RECIBIR    

    cpi DATO_UART, '1'
    breq EXEC_TRIANGULO
    cpi DATO_UART, '2'
    breq EXEC_CIRCULO
    cpi DATO_UART, '3'
    breq EXEC_PENTAGRAMA
    cpi DATO_UART, '4'
    breq EXEC_LIBRE
    cpi DATO_UART, '5'
    breq EXEC_DITTO
    cpi DATO_UART, 'p'
    breq EXEC_DITTO
    cpi DATO_UART, 'P'
    breq EXEC_DITTO

    ; Opciones para todas las figuras
    cpi DATO_UART, 't'
    breq EXEC_TODAS
    cpi DATO_UART, 'T'
    breq EXEC_TODAS

    ; Resetear Zonas e Interfaz
    cpi DATO_UART, 'r'
    breq RESET_ZONAS
    cpi DATO_UART, 'R'
    breq RESET_ZONAS

    rjmp MAIN_LOOP

RESET_ZONAS:
    ldi ZONA_ACTUAL, 1
    rcall HOMING
    rcall MOSTRAR_MENU_SERIAL
    rjmp MAIN_LOOP

EXEC_TRIANGULO:   rcall DIBUJAR_TRIANGULO
                  rjmp MAIN_LOOP
EXEC_CIRCULO:     rcall DIBUJAR_CIRCULO
                  rjmp MAIN_LOOP
EXEC_PENTAGRAMA:  rcall DIBUJAR_PENTAGRAMA
                  rjmp MAIN_LOOP
EXEC_LIBRE:       rcall DIBUJAR_LIBRE
                  rjmp MAIN_LOOP
EXEC_DITTO:       rcall DIBUJAR_DITTO
                  rjmp MAIN_LOOP

EXEC_TODAS:
    rcall DIBUJAR_TRIANGULO
    rcall DIBUJAR_CIRCULO
    rcall DIBUJAR_PENTAGRAMA
    rcall DIBUJAR_LIBRE
    rcall DIBUJAR_DITTO
    rjmp MAIN_LOOP


; ===== TRIÁNGULO RECTÁNGULO =====

DIBUJAR_TRIANGULO:
    rcall IR_A_ZONA_ACTUAL
    ESPERAR_1S

    IZQ 25
    ABJ 5

    BAJAR_LAPIZ
    ESPERAR_1S

    DER 20              ; Base
    ABJ 20              ; Altura
    DIAG_AI 20          ; Hipotenusa 

    LEVANTAR_LAPIZ
    rcall AVANZAR_ZONA
    ret


; ===== CÍRCULO =====

DIBUJAR_CIRCULO:
    rcall IR_A_ZONA_ACTUAL
    ESPERAR_1S

    IZQ 15
    ABJ 10

    BAJAR_LAPIZ
    ESPERAR_1S

    DER 5
    ABJ 1
    DER 4
    ABJ 1
    DER 2
    ABJ 1
    DER 2
    ABJ 1
    DER 1
    ABJ 1 
    DER 2
    ABJ 1
    DER 1
    ABJ 1
    DER 1
    ABJ 1
    DER 1
    ABJ 1
    DER 1
    ABJ 2
    DER 1
    ABJ 1
    DER 1
    ABJ 2
    DER 1
    ABJ 2
    DER 1
    ABJ 4
    DER 1
    ABJ 5
    ABJ 5
    IZQ 1
    ABJ 4
    IZQ 1
    ABJ 2
    IZQ 1
    ABJ 2
    IZQ 1
    ABJ 1
    IZQ 1
    ABJ 2
    IZQ 1
    ABJ 1
    IZQ 1
    ABJ 1
    IZQ 1
    ABJ 1
    IZQ 1
    ABJ 1
    IZQ 2
    ABJ 1
    IZQ 1
    ABJ 1
    IZQ 2
    ABJ 1
    IZQ 2
    ABJ 1
    IZQ 4
    ABJ 1
    IZQ 5
    IZQ 5
    ARR 1
    IZQ 4
    ARR 1
    IZQ 2
    ARR 1
    IZQ 2
    ARR 1
    IZQ 1
    ARR 1
    IZQ 2
    ARR 1
    IZQ 1
    ARR 1
    IZQ 1
    ARR 1
    IZQ 1
    ARR 1
    IZQ 1
    ARR 2
    IZQ 1
    ARR 1
    IZQ 1
    ARR 2
    IZQ 1
    ARR 2
    IZQ 1
    ARR 4
    IZQ 1
    ARR 5
    ARR 5
    DER 1
    ARR 4
    DER 1
    ARR 2
    DER 1
    ARR 2
    DER 1
    ARR 1
    DER 1
    ARR 2
    DER 1
    ARR 1
    DER 1
    ARR 1
    DER 1
    ARR 1
    DER 1
    ARR 1
    DER 2
    ARR 1
    DER 1
    ARR 1
    DER 2
    ARR 1
    DER 2
    ARR 1
    DER 2
    ARR 1
    DER 5

    LEVANTAR_LAPIZ
    rcall AVANZAR_ZONA
    ret


; ===== PENTAGRAMA =====

DIBUJAR_PENTAGRAMA:
    rcall IR_A_ZONA_ACTUAL
    ESPERAR_1S

    IZQ 20
    ABJ 3

    BAJAR_LAPIZ
    ESPERAR_1S

    DER 1
    ABJ 2
    DER 1
    ABJ 3
    DER 1
    ABJ 3
    DER 1
    ABJ 4
    DER 1
    ABJ 3
    DER 1
    ABJ 3
    DER 1
    ABJ 3
    DER 1
    ABJ 3
    DER 1
    ABJ 3
    DER 1
    ABJ 4
    DER 1
    ABJ 3
    DER 1
    ABJ 3
    DER 1
    ABJ 2
    IZQ 1
    ARR 1
    IZQ 2
    ARR 1
    IZQ 1
    ARR 1
    IZQ 1
    ARR 1
    IZQ 2
    ARR 1
    IZQ 1
    ARR 1
    IZQ 2
    ARR 1
    IZQ 1
    ARR 1
    IZQ 1
    ARR 1
    IZQ 2
    ARR 1
    IZQ 1
    ARR 1
    IZQ 2
    ARR 1
    IZQ 1
    ARR 1
    IZQ 1
    ARR 1
    IZQ 2
    ARR 1
    IZQ 1
    ARR 1
    IZQ 1
    ARR 1
    IZQ 2
    ARR 1
    IZQ 1
    ARR 1
    IZQ 2
    ARR 1
    IZQ 1
    ARR 1
    IZQ 1
    ARR 1
    IZQ 2
    ARR 1
    IZQ 1
    ARR 1
    DER 5
    DER 5
    DER 5
    DER 5
    DER 5
    DER 5
    DER 5
    DER 5
    DER 1
    ABJ 1
    IZQ 1
    ABJ 1
    IZQ 2
    ABJ 1
    IZQ 1
    ABJ 1
    IZQ 1
    ABJ 1
    IZQ 2
    ABJ 1
    IZQ 1
    ABJ 1
    IZQ 2
    ABJ 1
    IZQ 1
    ABJ 1
    IZQ 1
    ABJ 1
    IZQ 2
    ABJ 1
    IZQ 1
    ABJ 1
    IZQ 1
    ABJ 1
    IZQ 2
    ABJ 1
    IZQ 1
    ABJ 1
    IZQ 2
    ABJ 1
    IZQ 1
    ABJ 1
    IZQ 1
    ABJ 1
    IZQ 2
    ABJ 1
    IZQ 1
    ABJ 1
    IZQ 2
    ABJ 1
    IZQ 1
    ABJ 1
    IZQ 1
    ABJ 1
    IZQ 2
    ABJ 1
    IZQ 1
    ARR 2
    DER 1
    ARR 3
    DER 1
    ARR 3
    DER 1
    ARR 4
    DER 1
    ARR 3
    DER 1
    ARR 3
    DER 1
    ARR 3
    DER 1
    ARR 3
    DER 1
    ARR 3
    DER 1
    ARR 4
    DER 1
    ARR 3
    DER 1
    ARR 3
    DER 1

    LEVANTAR_LAPIZ
    rcall AVANZAR_ZONA
    ret


; =====DIBUJO LIBRE =====

DIBUJAR_LIBRE:
    rcall IR_A_ZONA_ACTUAL
    ESPERAR_1S

    IZQ 12
    ABJ 2

    BAJAR_LAPIZ
    ESPERAR_1S

    DER 5 
    ABJ 2
    DER 4
    ABJ 2
    DER 2
    ABJ 2
    DER 2
    ABJ 4
    DER 2
    ABJ 5
    ABJ 5
    IZQ 2
    ABJ 4
    IZQ 2
    ABJ 2
    IZQ 2
    ABJ 2
    IZQ 4
    ABJ 2
    IZQ 5
    IZQ 5
    ARR 2
    IZQ 4
    ARR 2
    IZQ 2
    ARR 2
    IZQ 2
    ARR 4
    IZQ 2
    ARR 5
    ARR 5
    DER 2
    ARR 4
    DER 2
    ARR 2
    DER 2
    ARR 2
    DER 4
    ARR 2
    DER 5

    LEVANTAR_LAPIZ
    ESPERAR_1S

    ABJ 5
    ABJ 5
    IZQ 5
    IZQ 5
    IZQ 3

    BAJAR_LAPIZ
    ESPERAR_1S

    DER 5
    DER 5
    DER 5
    DER 5
    DER 5
    DER 3

    LEVANTAR_LAPIZ
    ESPERAR_1S
    ABJ 2

    BAJAR_LAPIZ
    ESPERAR_1S

    IZQ 4
    ABJ 4
    IZQ 2
    ABJ 2
    IZQ 3
    IZQ 3
    ARR 2
    IZQ 2
    ARR 2
    IZQ 2
    ABJ 2
    IZQ 2
    ABJ 2
    IZQ 3
    IZQ 3
    ARR 2
    IZQ 2
    ARR 4
    IZQ 4

    LEVANTAR_LAPIZ
    ESPERAR_1S

    DER 5
    DER 3
    ABJ 3
    ABJ 5

    BAJAR_LAPIZ
    ESPERAR_1S

    DER 2
    ABJ 2
    DER 3
    DER 3
    ABJ 2
    IZQ 3
    IZQ 3
    ARR 2
    IZQ 2
    ARR 2

    LEVANTAR_LAPIZ
    rcall AVANZAR_ZONA
    ret

