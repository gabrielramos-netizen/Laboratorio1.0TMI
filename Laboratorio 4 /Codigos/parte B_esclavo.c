#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

// Comandos SPI
#define CMD_ESTADO    0x01
#define CMD_VELOCIDAD 0x02
#define CMD_PISTON    0x03
#define CMD_EMERG     0x04

// Definición de Salidas Digitales (LEDs)
#define LED_ROJO    PD4  // Indicador PARO
#define LED_VERDE   PD7  // Indicador MARCHA
#define LED_MOTOR   PD6  // Reemplazo Motor Cinta
#define LED_PISTON  PB1  // Reemplazo Servo Empujador (Pin D9)
#define BUZZER      PD3  // Alarma/Pitido

volatile uint8_t spi_byte_count = 0;
volatile uint8_t rx_cmd = 0;
volatile uint8_t rx_val = 0;
volatile uint8_t nueva_trama = 0;

uint8_t estado_actual = 0;

// Resincronización SPI cuando SS vuelve a HIGH
ISR(PCINT0_vect) {
	if (PINB & (1 << PB2)) {
		spi_byte_count = 0;
	}
}

// Recepción SPI por Interrupción
ISR(SPI_STC_vect) {
	uint8_t data = SPDR;
	if (spi_byte_count == 0) {
		rx_cmd = data;
		spi_byte_count = 1;
		} else if (spi_byte_count == 1) {
		rx_val = data;
		spi_byte_count = 0;
		nueva_trama = 1;
	}
}

void init_perifericos(void) {
	DDRD |= (1 << LED_ROJO) | (1 << LED_VERDE) | (1 << LED_MOTOR) | (1 << BUZZER);
	DDRB |= (1 << LED_PISTON);

	// Secuencia de prueba al arrancar
	for (uint8_t i = 0; i < 3; i++) {
		PORTD |= (1 << LED_ROJO);
		_delay_ms(150);
		PORTD &= ~(1 << LED_ROJO);
		_delay_ms(150);
	}

	PORTD |= (1 << LED_ROJO);
	PORTD &= ~((1 << LED_VERDE) | (1 << LED_MOTOR) | (1 << BUZZER));
	PORTB &= ~(1 << LED_PISTON);
}

void spi_init_slave(void) {
	DDRB |= (1 << PB4); // MISO salida
	DDRB &= ~((1 << PB2) | (1 << PB3) | (1 << PB5)); // Entradas SPI

	PCICR |= (1 << PCIE0);
	PCMSK0 |= (1 << PCINT2);

	SPCR = (1 << SPE) | (1 << SPIE);

	sei();
}

void disparar_piston(void) {
	PORTB |= (1 << LED_PISTON);
	PORTD |= (1 << BUZZER);
	_delay_ms(400);
	
	PORTB &= ~(1 << LED_PISTON);
	PORTD &= ~(1 << BUZZER);
}

int main(void) {
	init_perifericos();
	spi_init_slave();

	while (1) {
		if (nueva_trama) {
			nueva_trama = 0;

			switch (rx_cmd) {
				case CMD_ESTADO:
				estado_actual = rx_val;
				if (estado_actual == 0) { // PARO
					PORTD |= (1 << LED_ROJO);
					PORTD &= ~((1 << LED_VERDE) | (1 << LED_MOTOR));
					} else if (estado_actual == 1) { // MARCHA
					PORTD |= (1 << LED_VERDE) | (1 << LED_MOTOR);
					PORTD &= ~(1 << LED_ROJO);
				}
				break;

				case CMD_PISTON:
				if (estado_actual == 1) {
					disparar_piston();
				}
				break;

				case CMD_EMERG:
				estado_actual = 2; // EMERGENCIA
				PORTD &= ~((1 << LED_VERDE) | (1 << LED_MOTOR));
				PORTB &= ~(1 << LED_PISTON);
				break;
			}
		}

		if (estado_actual == 2) {
			PORTD ^= (1 << LED_ROJO) | (1 << BUZZER);
			PORTD &= ~(1 << LED_VERDE);
			_delay_ms(200);
		}
	}
}
