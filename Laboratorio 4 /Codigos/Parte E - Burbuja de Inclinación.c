#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "twi.h"
#include "UART.h"

#ifdef __cplusplus
}
#endif

// Definiciones de Hardware y Pines
#define PIN_MATRIZ_DATOS PORTD6  // Pin Digital 6
#define PIN_BUZZER       PORTD4  // Pin Digital 4

#define DIRECCION_MPU_WRITE 0xD0
#define DIRECCION_MPU_READ  0xD1
#define TOTAL_LEDS 64

// Variables Globales
int16_t angulo_x = 0;
int16_t angulo_y = 0;

uint8_t pos_x = 3;
uint8_t pos_y = 3;

uint8_t color_r = 0;
uint8_t color_g = 0;
uint8_t color_b = 255;

// --- Protocolo WS2812B (Ciclos de Reloj exactos a 16 MHz) ---
void enviar_byte_ws2812(uint8_t b) {
    for (uint8_t i = 0; i < 8; i++) {
        if (b & 0x80) {
            // Bit '1'
            PORTD |= (1 << PIN_MATRIZ_DATOS);
            __builtin_avr_delay_cycles(10);
            PORTD &= ~(1 << PIN_MATRIZ_DATOS);
            __builtin_avr_delay_cycles(5);
        } else {
            // Bit '0'
            PORTD |= (1 << PIN_MATRIZ_DATOS);
            __builtin_avr_delay_cycles(3);
            PORTD &= ~(1 << PIN_MATRIZ_DATOS);
            __builtin_avr_delay_cycles(9);
        }
        b <<= 1;
    }
}

void actualizar_matriz_rgb(void) {
    uint8_t led_activo = (pos_y * 8) + pos_x;

    cli();
    for (uint8_t i = 0; i < TOTAL_LEDS; i++) {
        if (i == led_activo) {
            enviar_byte_ws2812(color_g); // Formato GRB
            enviar_byte_ws2812(color_r);
            enviar_byte_ws2812(color_b);
        } else {
            enviar_byte_ws2812(0);
            enviar_byte_ws2812(0);
            enviar_byte_ws2812(0);
        }
    }
    sei();
    
    _delay_us(300);
}

// --- MPU6050 y Lógica de Nivelación ---
void iniciar_mpu6050(void) {
    twi_start();
    twi_write_cmd(DIRECCION_MPU_WRITE);
    twi_write_dwr(0x6B);
    twi_write_dwr(0x00);
    twi_stop();
    _delay_ms(50);
}

void calcular_angulos(void) {
    twi_start();
    twi_write_cmd(DIRECCION_MPU_WRITE);
    twi_write_dwr(0x3B);
    twi_repeated_start();
    twi_write_cmd(DIRECCION_MPU_READ);

    int16_t ax = ((int16_t)twi_read_ack() << 8) | (uint8_t)twi_read_ack();
    int16_t ay = ((int16_t)twi_read_ack() << 8) | (uint8_t)twi_read_ack();
    int16_t az = ((int16_t)twi_read_ack() << 8) | (uint8_t)twi_read_nack();
    twi_stop();

    if (az == 0 && ax == 0 && ay == 0) return;

    float pitch = atan2((float)ay, (float)az) * (180.0 / 3.14159);
    float roll  = atan2((float)-ax, (float)az) * (180.0 / 3.14159);

    angulo_x = (int16_t)pitch;
    angulo_y = (int16_t)roll;
}

void calcular_posicion_y_color(void) {
    int16_t px = 3 + (angulo_x / 10);
    int16_t py = 3 + (angulo_y / 10);

    if (px < 0) px = 0;
    if (px > 7) px = 7;
    if (py < 0) py = 0;
    if (py > 7) py = 7;

    pos_x = (uint8_t)px;
    pos_y = (uint8_t)py;

    int16_t dist = abs(angulo_x) + abs(angulo_y);

    if (dist < 4) {
        // Nivelado -> Verde
        color_r = 0; color_g = 255; color_b = 0;
        PORTD &= ~(1 << PIN_BUZZER);
    } else if (dist < 15) {
        // Inclinación leve -> Azul
        color_r = 0; color_g = 0; color_b = 255;
        PORTD &= ~(1 << PIN_BUZZER);
    } else {
        // Inclinación crítica -> Rojo y Alarma
        color_r = 255; color_g = 0; color_b = 0;
        PORTD |= (1 << PIN_BUZZER);
    }
}

void enviar_reporte_serie(void) {
    char texto_buffer[60];
    sprintf(texto_buffer, "PITCH: %d | ROLL: %d | [NORMAL]\r\n", angulo_x, angulo_y);
    uart_print(texto_buffer);
}

void setup() {
    DDRD |= (1 << PIN_BUZZER) | (1 << PIN_MATRIZ_DATOS);
    uart_init(103);
    PORTC |= (1 << PORTC4) | (1 << PORTC5);

    twi_init();
    iniciar_mpu6050();
}

void loop() {
    calcular_angulos();
    calcular_posicion_y_color();
    actualizar_matriz_rgb();
    enviar_reporte_serie();
    _delay_ms(50);
}
