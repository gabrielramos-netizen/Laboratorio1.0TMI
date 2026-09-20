.include "m328pdef.inc"

.def TEMP      = r16
.def DATO_UART = r17
.def DELAY1    = r18
.def DELAY2    = r19
.def PASOS     = r21
.def MULT      = r22
.def ACT_X     = r23      ; Posicion x real del plotter 
.def ACT_Y     = r24      ; Posicion y real del plotter
.def DEST_X    = r25      ; Coordenada x a donde ir
.def DEST_Y    = r20      ; Coordenada y a donde ir

.equ SOL_BAJAR  = (1<<2)   ; D2 -> X0 (Bajar solenoide)
.equ SOL_SUBIR  = (1<<3)   ; D3 -> X1 (Subir solenoide)
.equ MOV_ABAJO  = (1<<4)   ; D4 -> X5 (Mover abajo)
.equ MOV_ARRIBA = (1<<5)   ; D5 -> X6 (Mover arriba)
.equ MOV_IZQ    = (1<<6)   ; D6 -> X7 (Mover izquierda)
.equ MOV_DER    = (1<<7)   ; D7 -> X10 (Mover derecha)

.equ PASOS_HOMING = 255    
.equ SEPARACION   = 60     
.equ ESCALA       = 4      

.org 0x0000
    rjmp INICIO

INICIO:
    ldi TEMP, HIGH(RAMEND)
    out SPH, TEMP
    ldi TEMP, LOW(RAMEND)
    out SPL, TEMP

    ldi TEMP, 0xFC        
    out DDRD, TEMP
    clr TEMP
    out PORTD, TEMP      

    ;=====CONFIGURACIÓN DE USART=====;
    ldi TEMP, 0x00
    sts UBRR0H, TEMP
    ldi TEMP, 103      
    sts UBRR0L, TEMP

    ldi TEMP, (1<<RXEN0) | (1<<TXEN0)
    sts UCSR0B, TEMP

    ldi TEMP, (1<<UCSZ01) | (1<<UCSZ00)
    sts UCSR0C, TEMP

    rcall HOMING         

    rjmp MAIN_LOOP         

UART_RECIBIR:
    lds TEMP, UCSR0A
    sbrs TEMP, RXC0      
    rjmp UART_RECIBIR     
    lds DATO_UART, UDR0  
    ret

MAIN_LOOP:
    rcall UART_RECIBIR    

    cpi DATO_UART, '1'    
    breq IR_TRIANGULO

    cpi DATO_UART, '2'    
    breq IR_CIRCULO

    cpi DATO_UART, '3' 
    breq IR_PENTAGRAMA

    cpi DATO_UART, '4'   
    breq IR_LIBRE

    cpi DATO_UART, 'P'   
    breq IR_DITTO
    cpi DATO_UART, 'p'
    breq IR_DITTO

    cpi DATO_UART, 'T'  
    breq IR_TODAS
    cpi DATO_UART, 't'
    breq IR_TODAS

    rjmp MAIN_LOOP        

IR_TRIANGULO:   
    rcall D_TRIANGULO  
    rjmp MAIN_LOOP
IR_CIRCULO:     
    rcall D_CIRCULO    
    rjmp MAIN_LOOP
IR_PENTAGRAMA:  
    rcall D_PENTAGRAMA 
    rjmp MAIN_LOOP
IR_LIBRE:       
    rcall D_FIGURA_LIBRE      
    rjmp MAIN_LOOP
IR_DITTO:       
    rcall D_DITTO      
    rjmp MAIN_LOOP

IR_TODAS:
  
    rcall D_TRIANGULO
    ldi PASOS, SEPARACION
    rcall MOVER_IZQ_VACIO
    ldi TEMP, SEPARACION
    add ACT_X, TEMP

    rcall D_CIRCULO
    ldi PASOS, SEPARACION
    rcall MOVER_IZQ_VACIO
    ldi TEMP, SEPARACION
    add ACT_X, TEMP

    rcall D_PENTAGRAMA
    ldi PASOS, SEPARACION
    rcall MOVER_IZQ_VACIO
    ldi TEMP, SEPARACION
    add ACT_X, TEMP

    rcall D_FIGURA_LIBRE
    ldi PASOS, SEPARACION
    rcall MOVER_IZQ_VACIO
    ldi TEMP, SEPARACION
    add ACT_X, TEMP

    rcall D_DITTO
    rjmp MAIN_LOOP


D_TRIANGULO:
    ldi r27, high(TABLA_TRI*2)
    ldi r26, low(TABLA_TRI*2)
    rjmp EJECUTAR_TRAZO

D_CIRCULO:
    ldi r27, high(TABLA_CIRC*2)
    ldi r26, low(TABLA_CIRC*2)
    rjmp EJECUTAR_TRAZO

D_PENTAGRAMA:
    ldi r27, high(TABLA_PENTA*2)
    ldi r26, low(TABLA_PENTA*2)
    rjmp EJECUTAR_TRAZO

D_FIGURA_LIBRE:
    ldi r27, high(TABLA_LIBRE*2)
    ldi r26, low(TABLA_LIBRE*2)
    rjmp EJECUTAR_TRAZO

D_DITTO:
    ldi r27, high(TABLA_DITTO*2)
    ldi r26, low(TABLA_DITTO*2)
    rjmp EJECUTAR_TRAZO

EJECUTAR_TRAZO:
    mov ZH, r27
    mov ZL, r26

    lpm DEST_X, Z+
    lpm DEST_Y, Z+

    rcall IR_A_DESTINO     

    rcall BAJAR            
    rcall PASAR_PUNTOS   
    ret

