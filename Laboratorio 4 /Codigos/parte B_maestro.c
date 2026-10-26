#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdlib.h>

#include "twi.h"
#include "SPI.h"

void twi_start(void);

// Configuración LCD I2C (PCF8574T)
#define DIR_LCD       0x27
#define LCD_BACKLIGHT 0x08
#define ENABLE        0x04
#define RS_DATA       0x01
#define RS_CMD        0x00

// Umbral de temperatura máxima
#define TEMP_MAX      40

// Comandos SPI
#define CMD_ESTADO    0x01
#define CMD_VELOCIDAD 0x02
#define CMD_PISTON    0x03
#define CMD_EMERG     0x04

// Variables del sistema
uint8_t estado = 0;        // 0: PARO, 1: MARCHA, 2: EMERGENCIA
uint16_t piezas = 0;       // Contador de piezas
uint8_t flag_pieza = 0;    // Histéresis LDR
uint8_t pwm_anterior = 0;  // Registro de potenciómetro
uint16_t val_ldr_global = 0;


// DRIVER LCD
void pcf8574_write(uint8_t data) {
	twi_start();
	twi_write_cmd(DIR_LCD << 1);
	twi_write_dwr(data | LCD_BACKLIGHT);
	twi_stop();
}

void lcd_send_nibble(uint8_t nibble, uint8_t rs) {
	uint8_t data = (nibble & 0xF0) | rs | LCD_BACKLIGHT;
	pcf8574_write(data | ENABLE);
	_delay_us(2);
	pcf8574_write(data & ~ENABLE);
	_delay_us(50);
}

void lcd_cmd(uint8_t cmd) {
	lcd_send_nibble(cmd & 0xF0, RS_CMD);
	lcd_send_nibble((cmd << 4) & 0xF0, RS_CMD);
	if (cmd == 0x01 || cmd == 0x02) _delay_ms(2);
}

void lcd_data(uint8_t dato) {
	lcd_send_nibble(dato & 0xF0, RS_DATA);
	lcd_send_nibble((dato << 4) & 0xF0, RS_DATA);
}

void lcd_texto(char *txt) {
	while (*txt) lcd_data(*txt++);
}

void lcd_pos(uint8_t fila, uint8_t col) {
	uint8_t pos = (fila == 0) ? (0x80 + col) : (0xC0 + col);
	lcd_cmd(pos);
}

void lcd_init(void) {
	twi_init();
	_delay_ms(50);
	lcd_send_nibble(0x30, RS_CMD);
	_delay_ms(5);
	lcd_send_nibble(0x30, RS_CMD);
	_delay_us(150);
	lcd_send_nibble(0x30, RS_CMD);
	_delay_ms(1);
	lcd_send_nibble(0x20, RS_CMD);
	_delay_ms(1);

	lcd_cmd(0x28);
	lcd_cmd(0x0C);
	lcd_cmd(0x01);
	_delay_ms(2);
	lcd_cmd(0x06);
}

// DRIVER ADC 
void adc_init(void) {
	ADMUX = (1 << REFS0);
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

uint16_t leer_adc(uint8_t canal) {
	ADMUX = (1 << REFS0) | (canal & 0x0F);
	_delay_us(100); // Tiempos de estabilización entre cambio de canales (A0 y A3)
	
	ADCSRA |= (1 << ADSC); // Primera lectura de purga
	while (ADCSRA & (1 << ADSC));
	
	_delay_us(50);
	ADCSRA |= (1 << ADSC); // Segunda lectura real
	while (ADCSRA & (1 << ADSC));
	return ADC;
}

// DHT11 
uint8_t leer_temp_dht11(uint8_t temp_previa) {
	uint8_t datos[5] = {0};
	uint16_t timeout;

	DDRD |= (1 << PD3);
	PORTD &= ~(1 << PD3);
	_delay_ms(18);
	PORTD |= (1 << PD3);
	_delay_us(30);

	DDRD &= ~(1 << PD3);
	_delay_us(40);

	// Si el sensor no responde, retorna 25°C seguros para no disparar falsa emergencia
	if (PIND & (1 << PD3)) return 25;

	timeout = 10000;
	while (!(PIND & (1 << PD3))) { if (--timeout == 0) return 25; }
	timeout = 10000;
	while (PIND & (1 << PD3)) { if (--timeout == 0) return 25; }

	for (uint8_t j = 0; j < 5; j++) {
		for (int8_t i = 7; i >= 0; i--) {
			timeout = 10000;
			while (!(PIND & (1 << PD3))) { if (--timeout == 0) return 25; }
			_delay_us(30);
			if (PIND & (1 << PD3)) {
				datos[j] |= (1 << i);
				timeout = 10000;
				while (PIND & (1 << PD3)) { if (--timeout == 0) return 25; }
			}
		}
	}

	uint8_t suma = datos[0] + datos[1] + datos[2] + datos[3];
	if ((datos[4] == suma) && (datos[2] > 0) && (datos[2] <= 80)) {
		return datos[2]; // Retorna lectura real si el sensor funciona
	}
	
	return 25;
}


// COMUNICACIÓN SPI MAESTRO
void spi_transfer_safe(uint8_t data) {
	SPDR = data;
	uint16_t timeout = 10000;
	while (!(SPSR & (1 << SPIF))) {
		if (--timeout == 0) break;
	}
}

void enviar_spi(uint8_t cmd, uint8_t val) {
	DDRB |= (1 << PB2) | (1 << PB3) | (1 << PB5);
	SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR1) | (1 << SPR0); // 125 kHz

	PORTB &= ~(1 << PB2); // SS LOW
	_delay_us(100);
	
	spi_transfer_safe(cmd);
	_delay_us(200);
	spi_transfer_safe(val);
	
	_delay_us(100);
	PORTB |= (1 << PB2);  // SS HIGH
	_delay_us(300);
}

// MAIN 
int main(void) {
	char txt[17];
	uint8_t temp = 25;
	uint8_t btn_anterior = 1;
	uint16_t timer_dht11 = 0;
	uint16_t timer_lcd = 0;

	DDRD &= ~(1 << PD2);
	PORTD |= (1 << PD2); // Pull-up interno para Botón Start/Stop

	DDRB |= (1 << PB2) | (1 << PB3) | (1 << PB5);
	PORTB |= (1 << PB2);

	adc_init();
	lcd_init();
	spi_init();

	_delay_ms(100);

	while (1) {
		// Muestreo DHT11 cada 1.5s
		timer_dht11 += 20;
		if (timer_dht11 >= 1500) {
			timer_dht11 = 0;
			temp = leer_temp_dht11(temp);
		}

		// Control de Sobretemperatura
		if (temp >= TEMP_MAX) {
			if (estado != 2) {
				estado = 2;
				enviar_spi(CMD_EMERG, 1);
			}
			} else if (estado == 2 && temp < TEMP_MAX) {
			estado = 0;
			enviar_spi(CMD_ESTADO, 0);
		}

		// Lectura Botón Start/Stop (PD2)
		uint8_t btn_actual = (PIND & (1 << PD2)) ? 1 : 0;
		if (btn_anterior == 1 && btn_actual == 0) {
			_delay_ms(30);
			if (!(PIND & (1 << PD2))) {
				if (estado == 0) {
					estado = 1;
					enviar_spi(CMD_ESTADO, 1);
					} else if (estado == 1) {
					estado = 0;
					enviar_spi(CMD_ESTADO, 0);
				}
			}
		}
		btn_anterior = btn_actual;

		// Lectura de LDR Pin A3 
		val_ldr_global = leer_adc(3);

		if (estado == 1) { // MARCHA
			// Lectura de Potenciómetro asignada al Pin A0
			uint16_t adc_pot = leer_adc(0);
			uint8_t pwm = adc_pot >> 2;

			if (abs((int)pwm - (int)pwm_anterior) > 3) {
				enviar_spi(CMD_VELOCIDAD, pwm);
				pwm_anterior = pwm;
			}

			// Detección LDR
			if (val_ldr_global < 500 && !flag_pieza) {
				flag_pieza = 1;
				piezas++;
				enviar_spi(CMD_PISTON, 1);
				} else if (val_ldr_global > 700) {
				flag_pieza = 0;
			}
		}

		// Refresco de Pantalla cada 200ms
		timer_lcd += 20;
		if (timer_lcd >= 200) {
			timer_lcd = 0;

			if (estado == 2) {
				lcd_pos(0, 0);
				lcd_texto("ALERTA: OVERTEMP");
				lcd_pos(1, 0);
				sprintf(txt, "TEMP:%2dC  EMERG!", temp);
				lcd_texto(txt);
				} else if (estado == 1) {
				lcd_pos(0, 0);
				sprintf(txt, "MARCHA   T:%2dC  ", temp);
				lcd_texto(txt);

				lcd_pos(1, 0);
				sprintf(txt, "L:%04d  PZ:%03d ", val_ldr_global, piezas);
				lcd_texto(txt);
				} else { // PARO
				lcd_pos(0, 0);
				lcd_texto("SISTEMA: PARO   ");

				lcd_pos(1, 0);
				sprintf(txt, "T:%2dC    PZ:%03d ", temp, piezas);
				lcd_texto(txt);
			}
		}

		_delay_ms(20);
	}
}
