;Esta es la parte A - Matriz de leds 

.include "m328pdef.inc" 
.org 0x0000
   rjmp inicio 
.org 0x0024; Vector de Recepción Completa USART (USART_RX), obtenido del datasheet del Atmega328p
   rjmp rx_interrupcion 

 ;Inicializamos la pila para las subrutinas 
 inicio: 
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

;Estado inicial por defecto: Modo '1' (Mensaje desplazable)
 ldi r20, '1'

 sei ;Habilitar interrupciones globales

 rcall mostrar_menu ; Enviar el menú por la UART al iniciar

 ; --- BUCLE PRINCIPAL (Evaluación del Modo) ---
bucle_principal:
    cpi r20, '0'
    breq modo_desplazamiento

    cpi r20, '1'
    breq modo_carita_sonriendo

    cpi r20, '2'
    breq modo_carita_guinando

    cpi r20, '3'
    breq modo_corazon

    cpi r20, '4'
    breq modo_carita_3

    cpi r20, '5'
    breq modo_asterisco

    cpi r20, '6'
    breq modo_carita_xd

    rjmp bucle_principal

; TABLAS DE BÚSQUEDA EN FLASH (LUT)
.ORG 0x0200

texto_menu:
    .DB "=== MENU MATRIZ LED ===", 10, 13
    .DB "0. Mensaje Desplazable", 10, 13
    .DB "1. Carita Sonriendo", 10, 13
    .DB "2. Carita Guinando", 10, 13
    .DB "3. Corazon", 10, 13
    .DB "4. Carita :3", 10, 13
    .DB "5. Asterisco", 10, 13
    .DB "6. Carita XD", 10, 13, 0

; Figuras de 8 bytes (cada byte representa una fila de 8 LEDs)
figura_sonriendo:
    .DB 0b00000000, 0b01100110, 0b01100110, 0b00000000, 0b00000000, 0b01000010, 0b00111100, 0b00000000

figura_guinando:
    .DB 0b00000000, 0b01100000, 0b01100110, 0b00000000, 0b00000000, 0b01000010, 0b00111100, 0b00000000

figura_corazon:
    .DB 0b00000000, 0b01100110, 0b11111111, 0b11111111, 0b01111110, 0b00111100, 0b00011000, 0b00000000

figura_carita_3:
    .DB 0b00000000, 0b01100110, 0b01100110, 0b00000000, 0b00110011, 0b01001100, 0b00110011, 0b00000000

figura_asterisco:
    .DB 0b00010000, 0b10010001, 0b01010010, 0b00111100, 0b00111100, 0b01010010, 0b10010001, 0b00010000

figura_xd:
    .DB 0b00000000, 0b01000100, 0b00100100, 0b00010110, 0b00010101, 0b00100100, 0b01000100, 0b00000000

; ==============================================================================
; MANEJO DE MODOS DE IMAGEN ESTÁTICA
; ==============================================================================

modo_carita_sonriendo:
    ; Carga la dirección de la carita sonriendo en el puntero Z (ZH:ZL).
    ; Se usa '<< 1' (multiplicar x2) para convertir la dirección de palabras 
    ; (16 bits) a dirección de bytes, necesaria para la instrucción LPM.
    LDI ZH, HIGH(figura_sonriendo << 1) ; Parte alta de la dirección en bytes
    LDI ZL, LOW(figura_sonriendo << 1)  ; Parte baja de la dirección en bytes
    RCALL refrescar_matriz              ; Dibuja en los LEDs los datos apuntados por Z
    RJMP bucle_principal                ; Vuelve a la espera para evaluar nuevas órdenes UART

modo_carita_guinando:
    ; Carga la dirección de la carita guiñando en bytes en el puntero Z
    LDI ZH, HIGH(figura_guinando << 1)  ; Parte alta (dirección x 2)
    LDI ZL, LOW(figura_guinando << 1)   ; Parte baja (dirección x 2)
    RCALL refrescar_matriz              ; Llama a la subrutina de multiplexado/barrido
    RJMP bucle_principal                ; Regresa al bucle de menú

modo_corazon:
    ; Carga la dirección de la figura del corazón en bytes en el puntero Z
    LDI ZH, HIGH(figura_corazon << 1)   ; Parte alta (dirección x 2)
    LDI ZL, LOW(figura_corazon << 1)    ; Parte baja (dirección x 2)
    RCALL refrescar_matriz              ; Dibuja el corazón en la matriz de LEDs
    RJMP bucle_principal                ; Regresa al bucle de menú

modo_carita_3:
    ; Carga la dirección de la carita ":3" en bytes en el puntero Z
    LDI ZH, HIGH(figura_carita_3 << 1)  ; Parte alta (dirección x 2)
    LDI ZL, LOW(figura_carita_3 << 1)   ; Parte baja (dirección x 2)
    RCALL refrescar_matriz              ; Dibuja la carita ":3" en la matriz
    RJMP bucle_principal                ; Regresa al bucle de menú

modo_asterisco:
    ; Carga la dirección del asterisco en bytes en el puntero Z
    LDI ZH, HIGH(figura_asterisco << 1) ; Parte alta (dirección x 2)
    LDI ZL, LOW(figura_asterisco << 1)  ; Parte baja (dirección x 2)
    RCALL refrescar_matriz              ; Dibuja el asterisco en la matriz
    RJMP bucle_principal                ; Regresa al bucle de menú

modo_carita_xd:
    ; Carga la dirección de la carita "XD" en bytes en el puntero Z
    LDI ZH, HIGH(figura_xd << 1)        ; Parte alta (dirección x 2)
    LDI ZL, LOW(figura_xd << 1)         ; Parte baja (dirección x 2)
    RCALL refrescar_matriz              ; Dibuja la carita "XD" en la matriz
    RJMP bucle_principal                ; Regresa al bucle de menú
modo_desplazamiento:
    ; (Pendiente: Lógica de la marquesina con desplazamiento)
    RJMP bucle_principal
