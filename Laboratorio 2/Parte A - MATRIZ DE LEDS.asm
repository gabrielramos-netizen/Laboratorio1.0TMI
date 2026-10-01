.include "m328pdef.inc"


; MAPEO DE REGISTROS UTILIZADOS
; r16 -> Registro temporal principal
; r17 -> Registro temporal secundario
; r18 -> Registro temporal auxiliar
; r19 -> Índice de barrido de filas
; r24 -> Carácter recibido/enviado por UART

; VARIABLES EN MEMORIA SRAM
.dseg
.org 0x0100
BUFFER:             .byte 8     ; Buffer de pantalla (8 bytes), son 8 bytes, uno por cada fila.
FILA_BARRIDO:       .byte 1     ; Fila activa en el barrido (0 a 7), guarda qué fila está siendo mostrada actualmente.
MODO:               .byte 1     ; 0 = Desplazamiento, 1 = Figura estática
FIGURA:             .byte 1     ; Índice de la figura activa (0 a 5), guarda qué figura se está mostrando.
CARACTER_MSG:       .byte 1     ; Índice del carácter actual del mensaje
COLUMNA_MSG:        .byte 1     ; Columna actual dentro del carácter
CONTEO_DESPLAZAR:   .byte 1     ; Contador de tiempo para el desplazamiento
PERIODO_DESPLAZAR: .byte 1     ; Período / velocidad del desplazamiento
BANDERA_DESPLAZAR: .byte 1     ; Banderola para procesar desplazamiento


; VECTORES DE INTERRUPCIÓN
.cseg
.org 0x0000
    rjmp REINICIO

.org OC1Aaddr
    rjmp ISR_TEMPORIZADOR1_COMPA

.org INT_VECTORS_SIZE

; INICIALIZACIÓN DE HARDWARE Y VARIABLES
REINICIO:
    ; 1. Configurar Puntero de Pila (Stack Pointer)
    ldi r16, high(RAMEND)
    out SPH, r16
    ldi r16, low(RAMEND)
    out SPL, r16

    clr r1                      ; Registro r1 siempre guardado en cero

    ; 2. Configurar Puertos como Salida
    ; PORTB (PB0..PB5) -> Filas 1 a 6
    ldi r16, 0b00111111
    out DDRB, r16

    ; PORTD (PD2..PD7) -> Columnas 1 a 6 (PD0/PD1 reservados para UART RX/TX)
    ldi r16, 0b11111100
    out DDRD, r16

    ; PORTC (PC0..PC1 -> Filas 7,8 | PC2..PC3 -> Columnas 7,8)
    ldi r16, 0b00001111
    out DDRC, r16

    ; 3. Estado Inicial Seguro (Filas en BAJO = Apagadas, Cols en ALTO = Apagadas)
    clr r16
    out PORTB, r16              ; PB0..PB5 en 0

    in r16, PORTC
    andi r16, 0b11111100        ; PC0..PC1 en 0 (Filas ap)
    ori r16, 0b00001100         ; PC2..PC3 en 1 (Cols ap)
    out PORTC, r16

    in r16, PORTD
    ori r16, 0b11111100         ; PD2..PD7 en 1 (Cols ap)
    out PORTD, r16

    ; 4. Inicializar Variables SRAM
    clr r16
    sts FILA_BARRIDO, r16
    sts MODO, r16               ; Iniciar en modo Desplazamiento por defecto
    sts FIGURA, r16
    sts CARACTER_MSG, r16
    sts COLUMNA_MSG, r16
    sts CONTEO_DESPLAZAR, r16
    sts BANDERA_DESPLAZAR, r16

    ldi r16, 80                 ; Velocidad inicial de desplazamiento (~80 ms)
    sts PERIODO_DESPLAZAR, r16

    ; 5. Inicializar Periféricos
    rcall LIMPIAR_BUFFER
    rcall INICIALIZAR_UART
    rcall INICIALIZAR_TEMPORIZADOR1

    ; 6. Mostrar Menú por UART al iniciar
    ldi ZH, high(TEXTO_MENU<<1)
    ldi ZL, low(TEXTO_MENU<<1)
    rcall UART_ENVIAR_CADENA

    sei                         ; Habilitar interrupciones globales


; BUCLE PRINCIPAL
PRINCIPAL:
    rcall UART_CONSULTAR
    brcs PROCESAR_UART

    lds r16, MODO
    tst r16
    brne PRINCIPAL              ; Si es modo Figura estática, omite el desplazamiento

    lds r16, BANDERA_DESPLAZAR
    tst r16
    breq PRINCIPAL

    cli
    clr r16
    sts BANDERA_DESPLAZAR, r16
    sei

    rcall PASO_DESPLAZAMIENTO
    rjmp PRINCIPAL


; PROCESAMIENTO DE COMANDOS UART

PROCESAR_UART:
    cpi r24, 'M'
    breq ESTABLECER_MODO_MENSAJE
    cpi r24, 'm'
    breq ESTABLECER_MODO_MENSAJE

    ; Controles de Velocidad
    cpi r24, '+'
    breq AUMENTAR_VELOCIDAD
    cpi r24, 'F'
    breq AUMENTAR_VELOCIDAD
    cpi r24, 'f'
    breq AUMENTAR_VELOCIDAD

    cpi r24, '-'
    breq DISMINUIR_VELOCIDAD
    cpi r24, 'S'
    breq DISMINUIR_VELOCIDAD
    cpi r24, 's'
    breq DISMINUIR_VELOCIDAD

    ; Selección de figuras (1..6)
    cpi r24, '1'
    brlo PRINCIPAL
    cpi r24, '7'
    brsh PRINCIPAL

    ; Si presionó un número entre '1' y '6'
    mov r16, r24
    subi r16, '1'
    sts FIGURA, r16

    ldi r16, 1                  ; Cambiar a MODO = 1 (Figura estática)
    sts MODO, r16
    rcall CARGAR_FIGURA
    rjmp PRINCIPAL

ESTABLECER_MODO_MENSAJE:
    clr r16
    sts MODO, r16               ; Cambiar a MODO = 0 (Desplazamiento)
    sts CARACTER_MSG, r16
    sts COLUMNA_MSG, r16
    rcall LIMPIAR_BUFFER
    rjmp PRINCIPAL

AUMENTAR_VELOCIDAD:
    lds r16, PERIODO_DESPLAZAR
    subi r16, 10                ; Reducir período = Más rápido
    cpi r16, 10                 ; Límite mínimo de velocidad (~10ms)
    brsh GUARDAR_VELOCIDAD
    ldi r16, 10
    rjmp GUARDAR_VELOCIDAD

DISMINUIR_VELOCIDAD:
    lds r16, PERIODO_DESPLAZAR
    subi r16, -10               ; Incrementar período = Más lento (+10)
    cpi r16, 250                ; Límite máximo de período (~250ms)
    brlo GUARDAR_VELOCIDAD
    ldi r16, 250

GUARDAR_VELOCIDAD:
    sts PERIODO_DESPLAZAR, r16
    rjmp PRINCIPAL


; INTERRUPCIÓN TIMER 1 (Refresco de Matriz - 1 kHz)

ISR_TEMPORIZADOR1_COMPA:
    push r0
    in r0, SREG
    push r0
    push r16
    push r17
    push r18
    push r19
    push ZL
    push ZH

    ; APAGAR SALIDAS (Anti-fantasmas) 
    ; Filas a 0 (PB0..PB5, PC0..PC1)
    clr r16
    out PORTB, r16
    in r16, PORTC
    andi r16, 0b11111100
    out PORTC, r16

    ; Columnas a 1 (PD2..PD7, PC2..PC3)
    in r16, PORTD
    ori r16, 0b11111100
    out PORTD, r16
    in r16, PORTC
    ori r16, 0b00001100
    out PORTC, r16

    ; LEER PATRÓN DE COLUMNAS PARA LA FILA ACTUAL 
    lds r19, FILA_BARRIDO       ; Fila activa (0 a 7)

    ldi ZH, high(BUFFER)
    ldi ZL, low(BUFFER)
    add ZL, r19
    adc ZH, r1
    ld r16, Z                   ; r16 contiene la máscara de 8 bits para las columnas

    com r16                     ; Invertir bits: '1' en buffer -> '0' en salida (Activa cátodo)

    ; ACTIVAR COLUMNAS 
    ; Cols 1..6 -> PD2..PD7
    mov r17, r16
    andi r17, 0b00111111
    lsl r17
    lsl r17                     ; Mover bits a PD2..PD7
    in r18, PORTD
    andi r18, 0b00000011        ; Conservar PD0 y PD1 (UART)
    or r18, r17
    out PORTD, r18

    ; Columnas 7..8 -> PC2..PC3
    mov r17, r16
    andi r17, 0b11000000
    lsr r17
    lsr r17
    lsr r17
    lsr r17                     ; Mover bits 6..7 a PC2..PC3
    in r18, PORTC
    andi r18, 0b11110011
    or r18, r17
    out PORTC, r18

    ; --- 4. ACTIVAR LA FILA CORRESPONDIENTE (Ánodo: 1 activa) ---
    ldi r17, 0x01
    mov r18, r19
    tst r18
    breq MASCARA_FILA_OK

DESPLAZAR_FILA:
    lsl r17
    dec r18
    brne DESPLAZAR_FILA

MASCARA_FILA_OK:
    ; Filas 1..6 -> PB0..PB5
    mov r18, r17
    andi r18, 0b00111111
    out PORTB, r18

    ; Filas 7..8 -> PC0..PC1
    mov r18, r17
    andi r18, 0b11110000
    lsr r18
    lsr r18
    lsr r18
    lsr r18                     ; Mover bits 6..7 a PC0..PC1
    in r16, PORTC
    andi r16, 0b11111100
    or r16, r18
    out PORTC, r16

    ; INCREMENTAR FASES DE BARRIDO 
    inc r19
    andi r19, 0x07              ; Mantener índice entre 0 y 7
    sts FILA_BARRIDO, r19

    ; CONTROL DE TIEMPO DE DESPLAZAMIENTO 
    lds r16, CONTEO_DESPLAZAR
    inc r16
    sts CONTEO_DESPLAZAR, r16

    lds r17, PERIODO_DESPLAZAR
    cp r16, r17
    brlo SALIDA_ISR

    clr r16
    sts CONTEO_DESPLAZAR, r16
    ldi r16, 1
    sts BANDERA_DESPLAZAR, r16

SALIDA_ISR:
    pop ZH
    pop ZL
    pop r19
    pop r18
    pop r17
    pop r16
    pop r0
    out SREG, r0
    pop r0
    reti


; RUTINAS

INICIALIZAR_UART:
    ldi r16, (1<<U2X0)
    sts UCSR0A, r16
    ldi r16, high(207)
    sts UBRR0H, r16
    ldi r16, low(207)
    sts UBRR0L, r16
    ldi r16, (1<<RXEN0)|(1<<TXEN0)
    sts UCSR0B, r16
    ldi r16, (1<<UCSZ01)|(1<<UCSZ00)
    sts UCSR0C, r16
    ret

UART_CONSULTAR:
    lds r16, UCSR0A
    sbrs r16, RXC0
    rjmp SIN_DATOS
    lds r24, UDR0
    sec
    ret
SIN_DATOS:
    clc
    ret

UART_ENVIAR_CARACTER:
ESPERAR_UDRE:
    lds r16, UCSR0A
    sbrs r16, UDRE0
    rjmp ESPERAR_UDRE
    sts UDR0, r24
    ret

UART_ENVIAR_CADENA:
    lpm r24, Z+
    tst r24
    breq FIN_CADENA
    rcall UART_ENVIAR_CARACTER
    rjmp UART_ENVIAR_CADENA
FIN_CADENA:
    ret

INICIALIZAR_TEMPORIZADOR1:
    ldi r16, 0
    sts TCCR1A, r16
    ldi r16, (1<<WGM12)|(1<<CS11)|(1<<CS10) ; Prescaler 64
    sts TCCR1B, r16
    ldi r16, high(249)
    sts OCR1AH, r16
    ldi r16, low(249)
    sts OCR1AL, r16
    ldi r16, (1<<OCIE1A)
    sts TIMSK1, r16
    ret

LIMPIAR_BUFFER:
    ldi YH, high(BUFFER)
    ldi YL, low(BUFFER)
    clr r16
    ldi r17, 8
BUCLE_LIMPIAR:
    st Y+, r16
    dec r17
    brne BUCLE_LIMPIAR
    ret

CARGAR_FIGURA:
    lds r17, FIGURA
    lsl r17
    lsl r17
    lsl r17
    ldi ZH, high(FIGURAS<<1)
    ldi ZL, low(FIGURAS<<1)
    add ZL, r17
    adc ZH, r1
    ldi YH, high(BUFFER)
    ldi YL, low(BUFFER)
    ldi r18, 8
BUCLE_CARGAR:
    lpm r16, Z+
    st Y+, r16
    dec r18
    brne BUCLE_CARGAR
    ret

PASO_DESPLAZAMIENTO:
OBTENER_CARACTER:
    lds r18, CARACTER_MSG

    ldi ZH, high(MENSAJE<<1)
    ldi ZL, low(MENSAJE<<1)
    add ZL, r18
    adc ZH, r1

    lpm r16, Z

    tst r16
    brne CARACTER_CORRECTO

    clr r18
    sts CARACTER_MSG, r18
    sts COLUMNA_MSG, r18
    rjmp OBTENER_CARACTER

CARACTER_CORRECTO:
    cpi r16, ' '
    breq COLUMNA_VACIA
    cpi r16, 'A'
    brlo COLUMNA_VACIA
    cpi r16, 'Z'+1
    brsh COLUMNA_VACIA

    subi r16, 'A'

    ; Offset = Carácter * 6 bytes
    ldi r17, 6
    mul r16, r17
    mov r17, r0
    clr r1

    lds r18, COLUMNA_MSG
    add r17, r18

    ldi ZH, high(FUENTES<<1)
    ldi ZL, low(FUENTES<<1)
    add ZL, r17
    adc ZH, r1

    lpm r16, Z
    rjmp INSERTAR_COLUMNA

COLUMNA_VACIA:
    clr r16

INSERTAR_COLUMNA:
    ldi YH, high(BUFFER)
    ldi YL, low(BUFFER)
    clr r17

INSERTAR_FILA:
    ld r18, Y
    lsr r18                  

    sbrc r16, 0
    ori r18, 0b10000000      

    st Y+, r18
    lsr r16

    inc r17
    cpi r17, 8
    brlo INSERTAR_FILA

    lds r17, COLUMNA_MSG
    inc r17
    cpi r17, 6
    brlo GUARDAR_POSICION_MENSAJE

    clr r17
    lds r18, CARACTER_MSG
    inc r18
    sts CARACTER_MSG, r18

GUARDAR_POSICION_MENSAJE:
    sts COLUMNA_MSG, r17
    ret

; TABLAS EN MEMORIA FLASH
TEXTO_MENU:
    .DB 13, 10, "=== MENU ===", 13, 10, "M: Scroll", 13, 10, "+ / F: Vel. mas rapida", 13, 10, "- / S: Vel. mas lenta", 13, 10, "1: XD", 13, 10, "2: Feliz", 13, 10, "3: Guino", 13, 10, "4: Corazon", 13, 10, "5: Estrella", 13, 10, "6: Seria", 13, 10, 0, 0

MENSAJE:
    .DB "WELCOME HOME ",0,0

FIGURAS:
; 1: XD 
.DB 0b00000000         
.DB 0b01000010         
.DB 0b00100100        
.DB 0b01000010          
.DB 0b00000000          
.DB 0b00111100          
.DB 0b00100010         
.DB 0b00011100          

; 2: Cara Feliz
.DB 0b00111100, 0b01000010, 0b10100101, 0b10000001
.DB 0b10100101, 0b10011001, 0b01000010, 0b00111100

; 3: Guiño
.DB 0b00111100, 0b01000010, 0b10110101, 0b10000001
.DB 0b10100101, 0b10011001, 0b01000010, 0b00111100

; 4: Corazón
.DB 0b00000000, 0b01100110, 0b11111111, 0b11111111
.DB 0b11111111, 0b01111110, 0b00111100, 0b00011000

; 5: Estrella
.DB 0b00011000, 0b10011001, 0b01011010, 0b00111100
.DB 0b00111100, 0b01011010, 0b10011001, 0b00011000

; 6: Cara Seria
.DB 0b00111100, 0b01000010, 0b10100101, 0b10000001
.DB 0b10111101, 0b10000001, 0b01000010, 0b00111100

FUENTES:
; Fuente 5x7 (A..Z)
    .DB 0b01111110,0b00001001,0b00001001,0b00001001,0b01111110,0
    .DB 0b01111111,0b01001001,0b01001001,0b01001001,0b00110110,0
    .DB 0b00111100,0b01000001,0b01000001,0b01000001,0b01000001,0
    .DB 0b01111111,0b01000001,0b01000001,0b01000001,0b00111100,0
    .DB 0b01111111,0b01001001,0b01001001,0b01001001,0b01000001,0
    .DB 0b01111111,0b00001001,0b00001001,0b00001001,0b00000001,0
    .DB 0b00111100,0b01000001,0b01001001,0b01001001,0b01111001,0
    .DB 0b01111111,0b00001000,0b00001000,0b00001000,0b01111111,0
    .DB 0b01000001,0b01000001,0b01111111,0b01000001,0b01000001,0
    .DB 0b00100000,0b01000000,0b01000001,0b00111111,0b00000001,0
    .DB 0b01111111,0b00001000,0b00010100,0b00100010,0b01000001,0
    .DB 0b01111111,0b01000000,0b01000000,0b01000000,0b01000000,0
    .DB 0b01111111,0b00000010,0b00001100,0b00000010,0b01111111,0
    .DB 0b01111111,0b00000010,0b00001100,0b00010000,0b01111111,0
    .DB 0b00111100,0b01000001,0b01000001,0b01000001,0b00111100,0
    .DB 0b01111111,0b00001001,0b00001001,0b00001001,0b00000110,0
    .DB 0b00111100,0b01000001,0b01010001,0b00100001,0b01011110,0
    .DB 0b01111111,0b00001001,0b00011001,0b00201001,0b01000110,0
    .DB 0b01000110,0b01001001,0b01001001,0b01001001,0b00110001,0
    .DB 0b00000001,0b00000001,0b01111111,0b00000001,0b00000001,0
    .DB 0b00111100,0b01000000,0b01000000,0b01000000,0b00111100,0
    .DB 0b00011111,0b00100000,0b01000000,0b00100000,0b00011111,0
    .DB 0b00111101,0b01100000,0b00011000,0b01100000,0b00111111,0
    .DB 0b01100011,0b00010100,0b00001000,0b00010100,0b01100011,0
    .DB 0b00000011,0b00000100,0b01111000,0b00000100,0b00000011,0
    .DB 0b01100001,0b01010001,0b01001001,0b01000101,0b01000011,0