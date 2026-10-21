#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdlib.h>

#include "twi.h"
#include "SPI.h"

void twi_start(void);
#define DIR_LCD 0x3E

// Comandos SPI
#define CMD_ESTADO    0x01
#define CMD_VELOCIDAD 0x02

uint8_t estado = 0;       // 0: Paro, 1: Marcha
uint8_t pwm_anterior = 0;

// Envío de comando al LCD
void lcd_cmd(uint8_t cmd) {
    twi_start();
    twi_write_cmd(DIR_LCD << 1);
    twi_write_dwr(0x00);
    twi_write_dwr(cmd);
    twi_stop();
    _delay_us(200);
}

// Envío de carácter al LCD
void lcd_data(uint8_t dato) {
    twi_start();
    twi_write_cmd(DIR_LCD << 1);
    twi_write_dwr(0x40);
    twi_write_dwr(dato);
    twi_stop();
    _delay_us(200);
}

void lcd_texto(char *txt) {
    while (*txt) {
        lcd_data(*txt++);
    }
}

// Inicialización de pantalla LCD
void lcd_init(void) {
    twi_init();
    _delay_ms(50);
    lcd_cmd(0x38);
    _delay_ms(5);
    lcd_cmd(0x0C);
    _delay_ms(5);
    lcd_cmd(0x01);
    _delay_ms(5);
    lcd_cmd(0x06);
}

// Configuración del ADC
void adc_init(void) {
    ADMUX = (1 << REFS0);
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

// Lectura de canal analógico
uint16_t leer_adc(uint8_t canal) {
    ADMUX = (1 << REFS0) | (canal & 0x0F);
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    return ADC;
}

// Transmisión SPI de comando y valor
void enviar_spi(uint8_t cmd, uint8_t val) {
    PORTB &= ~(1 << PB2); // Selección de esclavo (SS bajo)
    _delay_us(20);
    
    spi_transfer(cmd);
    _delay_us(50);
    spi_transfer(val);
    
    _delay_us(20);
    PORTB |= (1 << PB2);  // Deselección de esclavo (SS alto)
}

int main(void) {
    uint8_t btn_anterior = 1;

    // Configuración del botón de marcha/paro en PD2
    DDRD &= ~(1 << PD2);
    PORTD |= (1 << PD2);

    // Configuración del pin SS para SPI
    DDRB |= (1 << PB2);
    PORTB |= (1 << PB2);

    adc_init();
    lcd_init();
    spi_init();

    _delay_ms(100);

    while (1) {
        // Lectura de botón con filtro antirrebote
        uint8_t btn_actual = (PIND & (1 << PD2)) ? 1 : 0;
        if (btn_anterior == 1 && btn_actual == 0) {
            _delay_ms(50);
            if (!(PIND & (1 << PD2))) {
                if (estado == 0) {
                    estado = 1;
                    enviar_spi(CMD_ESTADO, 1);
                } else {
                    estado = 0;
                    enviar_spi(CMD_ESTADO, 0);
                }
            }
        }
        btn_anterior = btn_actual;

        if (estado == 1) { // Modo MARCHA
            uint16_t adc_pot = leer_adc(0);
            uint8_t pwm = adc_pot >> 2;
            
            // Envío por SPI únicamente al cambiar el valor
            if (abs((int)pwm - (int)pwm_anterior) > 3) {
                enviar_spi(CMD_VELOCIDAD, pwm);
                pwm_anterior = pwm;
            }
        }

        _delay_ms(100);
    }
}
