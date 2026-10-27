#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <stdio.h>
#include <stdlib.h>     // Necesario para la función abs()
#include <math.h>

#include "twi.h"
#include "UART.h"

// Definición de Hardware y Pines (Puerto D)
#define PIN_MATRIZ_DATOS PORTD6  // Pin Digital 6
#define PIN_BUZZER       PORTD4  // Pin Digital 4
#define PIN_BOTON_PAUSA   PORTD2  // Pin Digital 2
#define PIN_BOTON_CALIBRAR PORTD3 // Pin Digital 3

#define DIRECCION_MPU_WRITE 0xD0
#define DIRECCION_MPU_READ  0xD1
#define TOTAL_LEDS 64

// Direcciones de almacenamiento en memoria EEPROM
#define DIR_EEPROM_OFF_X 0x00
#define DIR_EEPROM_OFF_Y 0x04

// Variables globales de estado
int16_t angulo_x = 0;
int16_t angulo_y = 0;
float offset_x = 0;
float offset_y = 0;

uint8_t pos_x = 3;
uint8_t pos_y = 3;

uint8_t color_r = 0;
uint8_t color_g = 0;
uint8_t color_b = 255;

uint8_t en_pausa = 0;

//  WS2812B por precisión de ciclos de reloj (16 MHz)
void enviar_byte_ws2812(uint8_t b) {
    for (uint8_t i = 0; i < 8; i++) {
        if (b & 0x80) {
            // Bit '1': ~750ns ALTO, ~430ns BAJO
            PORTD |= (1 << PIN_MATRIZ_DATOS);
            __builtin_avr_delay_cycles(10);
            PORTD &= ~(1 << PIN_MATRIZ_DATOS);
            __builtin_avr_delay_cycles(5);
        } else {
            // Bit '0': ~310ns ALTO, ~700ns BAJO
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

    cli(); // Inhabilitar interrupciones durante el envío crítico
    for (uint8_t i = 0; i < TOTAL_LEDS; i++) {
        if (i == led_activo) {
            // Orden del chip WS2812B: Verde -> Rojo -> Azul
            enviar_byte_ws2812(color_g);
            enviar_byte_ws2812(color_r);
            enviar_byte_ws2812(color_b);
        } else {
            enviar_byte_ws2812(0);
            enviar_byte_ws2812(0);
            enviar_byte_ws2812(0);
        }
    }
    sei(); // Reactivar interrupciones
    
    _delay_us(300); // Señal de RESET / Latch
}

// --- Driver y Lectura MPU6050 ---
void iniciar_mpu6050(void) {
    twi_start();
    twi_write_cmd(DIRECCION_MPU_WRITE);
    twi_write_dwr(0x6B); // Registro PWR_MGMT_1
    twi_write_dwr(0x00); // Salir de modo Sleep
    twi_stop();
    _delay_ms(50);
}

void calcular_angulos(void) {
    twi_start();
    twi_write_cmd(DIRECCION_MPU_WRITE);
    twi_write_dwr(0x3B); // Registro ACCEL_XOUT_H
    twi_repeated_start();
    twi_write_cmd(DIRECCION_MPU_READ);

    int16_t ax = ((int16_t)twi_read_ack() << 8) | (uint8_t)twi_read_ack();
    int16_t ay = ((int16_t)twi_read_ack() << 8) | (uint8_t)twi_read_ack();
    int16_t az = ((int16_t)twi_read_ack() << 8) | (uint8_t)twi_read_nack();
    twi_stop();

    if (az == 0 && ax == 0 && ay == 0) return;

    float pitch = atan2((float)ay, (float)az) * (180.0 / 3.14159265);
    float roll  = atan2((float)-ax, (float)az) * (180.0 / 3.14159265);

    angulo_x = (int16_t)(pitch - offset_x);
    angulo_y = (int16_t)(roll - offset_y);
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

// --- Gestión de EEPROM y Entradas ---
void guardar_calibracion_eeprom(void) {
    offset_x += angulo_x;
    offset_y += angulo_y;
    eeprom_update_float((float*)DIR_EEPROM_OFF_X, offset_x);
    eeprom_update_float((float*)DIR_EEPROM_OFF_Y, offset_y);
}

void cargar_calibracion_eeprom(void) {
    offset_x = eeprom_read_float((float*)DIR_EEPROM_OFF_X);
    offset_y = eeprom_read_float((float*)DIR_EEPROM_OFF_Y);
    if (isnan(offset_x)) offset_x = 0;
    if (isnan(offset_y)) offset_y = 0;
}

void revisar_botones(void) {
    if (!(PIND & (1 << PIN_BOTON_PAUSA))) {
        _delay_ms(50);
        if (!(PIND & (1 << PIN_BOTON_PAUSA))) {
            en_pausa = !en_pausa;
            while (!(PIND & (1 << PIN_BOTON_PAUSA)));
        }
    }

    if (!(PIND & (1 << PIN_BOTON_CALIBRAR))) {
        _delay_ms(50);
        if (!(PIND & (1 << PIN_BOTON_CALIBRAR))) {
            guardar_calibracion_eeprom();
            while (!(PIND & (1 << PIN_BOTON_CALIBRAR)));
        }
    }
}

void enviar_reporte_serie(void) {
    char texto_buffer[60];
    if (en_pausa) {
        sprintf(texto_buffer, "PITCH: %d | ROLL: %d | [PAUSA]\r\n", angulo_x, angulo_y);
    } else {
        sprintf(texto_buffer, "PITCH: %d | ROLL: %d | [NORMAL]\r\n", angulo_x, angulo_y);
    }
    uart_print(texto_buffer);
}

// --- Configuración Inicial ---
void setup(void) {
    DDRD |= (1 << PIN_BUZZER) | (1 << PIN_MATRIZ_DATOS);
    
    DDRD &= ~((1 << PIN_BOTON_PAUSA) | (1 << PIN_BOTON_CALIBRAR));
    PORTD |= (1 << PIN_BOTON_PAUSA) | (1 << PIN_BOTON_CALIBRAR);

    uart_init(103); // UBRR = 103 para 9600 baudios a 16 MHz
    PORTC |= (1 << PORTC4) | (1 << PORTC5); // Habilitar Pull-ups I2C

    twi_init();
    iniciar_mpu6050();
    cargar_calibracion_eeprom();
}

// --- Bucle Continuo ---
void loop(void) {
    revisar_botones();

    if (!en_pausa) {
        calcular_angulos();
        calcular_posicion_y_color();
    }

    actualizar_matriz_rgb();
    enviar_reporte_serie();

    _delay_ms(50);
}

// --- Punto de Entrada Principal en C ---
int main(void) {
    setup();
    while (1) {
        loop();
    }
    return 0;
}
