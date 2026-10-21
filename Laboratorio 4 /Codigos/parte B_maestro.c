#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>

#include "twi.h"
#include "SPI.h"

void twi_start(void);
#define DIR_LCD 0x3E

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

int main(void) {
    // Configuración del botón de marcha/paro en PD2
    DDRD &= ~(1 << PD2);
    PORTD |= (1 << PD2);

    adc_init();
    lcd_init();

    lcd_texto("SISTEMA LISTO");

    while (1) {
        uint16_t pot = leer_adc(0);
        uint16_t ldr = leer_adc(1);
        _delay_ms(200);
    }
}
