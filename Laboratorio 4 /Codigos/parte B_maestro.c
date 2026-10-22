#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdlib.h>

#include "twi.h"
#include "SPI.h"

void twi_start(void);
#define DIR_LCD  0x3E
#define TEMP_MAX 40

// Comandos SPI
#define CMD_ESTADO    0x01
#define CMD_VELOCIDAD 0x02
#define CMD_PISTON    0x03
#define CMD_EMERG     0x04

uint8_t estado = 0;       // 0: Paro, 1: Marcha, 2: Emergencia
uint16_t piezas = 0;
uint8_t flag_pieza = 0;
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

// Posición del cursor en LCD
void lcd_pos(uint8_t fila, uint8_t col) {
    uint8_t pos = (fila == 0) ? (0x80 + col) : (0xC0 + col);
    lcd_cmd(pos);
}

// Configuración del ADC
void adc_init(void) {
    ADMUX = (1 << REFS0);
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

// Lectura de canal analógico con doble conversión para descarte de carga
uint16_t leer_adc(uint8_t canal) {
    ADMUX = (1 << REFS0) | (canal & 0x0F);
    _delay_us(50);
    
    ADCSRA |= (1 << ADSC); // Primera lectura (descarte)
    while (ADCSRA & (1 << ADSC));
    
    ADCSRA |= (1 << ADSC); // Segunda lectura (real)
    while (ADCSRA & (1 << ADSC));
    return ADC;
}

// Lectura del sensor de temperatura DHT11 en PD3
uint8_t leer_temp_dht11(void) {
    uint8_t datos[5] = {0};
    
    DDRD |= (1 << PD3);
    PORTD &= ~(1 << PD3);
    _delay_ms(18);
    PORTD |= (1 << PD3);
    _delay_us(30);
    
    DDRD &= ~(1 << PD3);
    _delay_us(40);
    
    if (PIND & (1 << PD3)) return 25;
    while (!(PIND & (1 << PD3)));
    while (PIND & (1 << PD3));
    
    for (uint8_t j = 0; j < 5; j++) {
        for (int8_t i = 7; i >= 0; i--) {
            while (!(PIND & (1 << PD3)));
            _delay_us(30);
            if (PIND & (1 << PD3)) {
                datos[j] |= (1 << i);
                while (PIND & (1 << PD3));
            }
        }
    }
    return datos[2];
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
    char txt[17];
    uint8_t temp = 25;
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
        temp = leer_temp_dht11();

        // Control de sobretemperatura (Emergencia)
        if (temp >= TEMP_MAX) {
            if (estado != 2) {
                estado = 2;
                enviar_spi(CMD_EMERG, 1);
            }
            
            lcd_pos(0, 0);
            lcd_texto("ALERTA: OVERTEMP");
            lcd_pos(1, 0);
            sprintf(txt, "TEMP:%2dC  EMERG!", temp);
            lcd_texto(txt);
            
            _delay_ms(400);
            continue;
        } else if (estado == 2 && temp < TEMP_MAX) {
            estado = 0;
            enviar_spi(CMD_ESTADO, 0);
        }

        // Lectura de botón con filtro antirrebote
        uint8_t btn_actual = (PIND & (1 << PD2)) ? 1 : 0;
        if (btn_anterior == 1 && btn_actual == 0) {
            _delay_ms(50);
            if (!(PIND & (1 << PD2))) {
                if (estado == 0) {
                    estado = 1;
                    enviar_spi(CMD_ESTADO, 1);
                    pwm_anterior = 255;
                } else if (estado == 1) {
                    estado = 0;
                    enviar_spi(CMD_ESTADO, 0);
                }
            }
        }
        btn_anterior = btn_actual;

        if (estado == 1) { // Modo MARCHA
            uint16_t adc_pot = leer_adc(0);
            uint8_t pwm = adc_pot >> 2;
            uint8_t vel_porcentaje = (adc_pot * 100UL) / 1023;
            
            // Envío por SPI únicamente al cambiar el valor
            if (abs((int)pwm - (int)pwm_anterior) > 3) {
                enviar_spi(CMD_VELOCIDAD, pwm);
                pwm_anterior = pwm;
            }

            // Detección de pieza mediante LDR
            uint16_t adc_ldr = leer_adc(1);
            if (adc_ldr < 1019 && !flag_pieza) {
                flag_pieza = 1;
                piezas++;
                enviar_spi(CMD_PISTON, 1);
            } else if (adc_ldr >= 1019) {
                flag_pieza = 0;
            }

            // Actualización de pantalla LCD
            lcd_pos(0, 0);
            sprintf(txt, "MARCHA   T:%2dC  ", temp);
            lcd_texto(txt);
            
            lcd_pos(1, 0);
            sprintf(txt, "V:%3d%%   PZ:%03d ", vel_porcentaje, piezas);
            lcd_texto(txt);
        } else if (estado == 0) { // Modo PARO
            lcd_pos(0, 0);
            lcd_texto("SISTEMA: PARO   ");
            
            lcd_pos(1, 0);
            sprintf(txt, "T:%2dC    PZ:%03d ", temp, piezas);
            lcd_texto(txt);
        }

        _delay_ms(150);
    }
}
