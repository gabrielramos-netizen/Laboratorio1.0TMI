// LABORATORIO 5 - PROBLEMA B: GESTIÓN ENERGÉTICA, WDT

#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <avr/power.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdio.h>

// Se establece la dirección I2C por defecto del expansor PCF8574 para controlar la pantalla LCD
#ifndef PCF8574
#define PCF8574 0x27
#endif

#include "twi.h"
#include "twi_lcd.h"

// Se definen la dirección de memoria EEPROM y la bandera mágica para recordar si el sistema sufrió un reinicio por Watchdog
#define EEPROM_WDT_FLAG_ADDR ((uint8_t*)0x00)
#define EEPROM_MAGIC_BYTE     0xA5

// Se ubica esta rutina en la sección .init3 para que se ejecute apenas arranca el microcontrolador antes del main
// Aca se limpia la bandera de reinicio por WDT y se apaga el perro guardián para evitar un bucle infinito de reinicios rápidos
void init_guardar_mcusr(void) __attribute__((naked, used, section(".init3")));
void init_guardar_mcusr(void) {
	MCUSR &= ~(1 << WDRF);
	wdt_disable();
}

// PROTOCOLO BLUETOOTH
// Se definen el tamaño del búfer circular y los bytes delimitadores de inicio y fin de trama
#define BUFFER_SIZE 32
#define HEADER_BYTE 0xAA
#define FOOTER_BYTE 0x55

// Se asignan los pines D4 y D5 para la comunicación serie por software con el fin de dejar libres los pines D0 y D1 del USB
// Se calcula un retardo de 104 us por bit para alcanzar una velocidad estándar de 9600 bps
#define BT_RX_PIN    PIND4
#define BT_TX_PIN    PORTD5
#define BIT_DELAY_US 104

// Se selecciona el canal analógico 1 correspondiente al pin físico A1 (PC1) donde se conecta el potenciómetro
#define POT_CANAL_ADC 1

// Se define un puntero a función para implementar la máquina de estados finitos sin recurrir a estructuras switch-case
typedef void (*EstadoFuncion_t)(void);

void st_activo(void);
void st_standby(void);
void st_apagado(void);

// Se declaran como volatile todas las variables compartidas entre las interrupciones y el bucle principal para evitar optimizaciones erróneas del compilador
volatile EstadoFuncion_t estado_actual = st_activo;
volatile uint8_t  codigo_estado_actual = 0;
volatile uint32_t contador_segundos_inactividad = 0;

volatile uint8_t bandera_solicitud_bloqueo = 0;
volatile uint8_t bandera_despertar_fsm = 0;
volatile uint8_t durmiendo = 0;
volatile uint8_t recibiendo_trama = 0;

// Se reservan variables globales para registrar los eventos de despertar y las lecturas obtenidas durante la Prueba A del WDT
volatile uint8_t  bandera_wdt_despertador = 0;
volatile uint16_t ultima_lectura_adc_wdt = 0;
volatile uint16_t contador_despertates_wdt = 0;

// Se crea un búfer circular para almacenar los bytes recibidos por Bluetooth sin perder datos mientras la CPU atiende otras tareas
volatile uint8_t rx_buffer[BUFFER_SIZE];
volatile uint8_t rx_head = 0;
volatile uint8_t rx_tail = 0;
volatile uint8_t ultimo_byte_rx = 0;

// Se guarda el último estado dibujado en la LCD para refrescar el texto fijo solo cuando ocurre una transición real y evitar parpadeos
static uint8_t ultimo_estado_lcd = 0xFF;

// SOFTWARE UART (9600 BPS)
// Aca se configuran los pines de la UART por software y se habilitan las interrupciones por cambio de pin en el puerto D
void SoftUART_Init(void) {
	// Se configura D4 como entrada con resistencia pull-up interna para mantener la línea RX en estado alto de reposo
	DDRD &= ~(1 << BT_RX_PIN);
	PORTD |= (1 << BT_RX_PIN);

	// Se configura D5 como salida y se pone en nivel alto porque el protocolo UART mantiene el reposo en 1 lógico
	DDRD |= (1 << BT_TX_PIN);
	PORTD |= (1 << BT_TX_PIN);

	// Se habilita el grupo de interrupciones PCINT2 para detectar pulsadores en D2/D3 y el bit de inicio en D4
	PCICR  |= (1 << PCIE2);
	PCMSK2 |= (1 << PCINT18) | (1 << PCINT19) | (1 << PCINT20);
}

// Aca se transmite un byte bit a bit generando manualmente los tiempos de inicio, datos y parada a 9600 bps
void SoftUART_Transmit(uint8_t data) {
	// Se deshabilitan temporalmente las interrupciones para que ninguna ISR altere la duración exacta de cada bit
	uint8_t sreg = SREG;
	cli();

	// Se envía el bit de inicio (Start) llevando la línea TX a nivel bajo durante 104 us
	PORTD &= ~(1 << BT_TX_PIN);
	_delay_us(BIT_DELAY_US);

	// Se transmiten los 8 bits de datos comenzando por el menos significativo (LSB)
	for (uint8_t i = 0; i < 8; i++) {
		if (data & (1 << i)) {
			PORTD |= (1 << BT_TX_PIN);
			} else {
			PORTD &= ~(1 << BT_TX_PIN);
		}
		_delay_us(BIT_DELAY_US);
	}

	// Se envía el bit de parada (Stop) devolviendo la línea TX a nivel alto
	PORTD |= (1 << BT_TX_PIN);
	_delay_us(BIT_DELAY_US);

	// Se restaura el estado previo de las interrupciones globales
	SREG = sreg;
}

// WDT ISR (PRUEBA A: DESPERTADOR)
// Aca se atiende la interrupción periódica de 8 segundos del Watchdog cuando el sistema duerme en modo Power-Down
ISR(WDT_vect) {
	// Solo se activa la bandera de muestreo si el microcontrolador se encuentra efectivamente en el estado APAGADO
	if (codigo_estado_actual == 2) {
		bandera_wdt_despertador = 1;
	}
}

// INT0 (PD2) - DESPERTAR EN PWR_DOWN
// Aca se captura la pulsación física por nivel bajo en D2, ya que en Power-Down los relojes están detenidos y se requiere disparo asíncrono
ISR(INT0_vect) {
	// Se deshabilita inmediatamente INT0 para evitar múltiples disparos mientras el dedo sigue presionando el pulsador
	EIMSK &= ~(1 << INT0);
	contador_segundos_inactividad = 0;
	bandera_despertar_fsm = 1;
}

// PCINT2: PULSADORES PD2/PD3 Y RECEPCIÓN BT EN PD4
// Aca se gestionan los cambios de estado en el puerto D para recibir tramas serie o detectar eventos de usuario
ISR(PCINT2_vect) {
	uint8_t estado_pind = PIND;

	// 1. Recepción Bluetooth en Pin D4
	// Se verifica si el pin D4 cayó a nivel bajo indicando el posible comienzo de un bit de inicio UART
	if (!(estado_pind & (1 << BT_RX_PIN))) {
		// Se espera medio tiempo de bit para muestrear exactamente en el centro y descartar ruidos breves en la línea
		_delay_us(BIT_DELAY_US / 2);

		if (!(PIND & (1 << BT_RX_PIN))) {
			uint8_t dato = 0;
			// Se leen los 8 bits sucesivos esperando un período completo entre cada muestra
			for (uint8_t i = 0; i < 8; i++) {
				_delay_us(BIT_DELAY_US);
				if (PIND & (1 << BT_RX_PIN)) {
					dato |= (1 << i);
				}
			}
			// Se espera el bit de parada para respetar el encuadre de la trama serie
			_delay_us(BIT_DELAY_US);

			ultimo_byte_rx = dato;
			if (dato == HEADER_BYTE) {
				recibiendo_trama = 1;
				} else if (dato == FOOTER_BYTE) {
				recibiendo_trama = 0;
			}

			// Se guarda el byte recibido dentro del búfer circular siempre que exista espacio disponible
			uint8_t next_head = (rx_head + 1) % BUFFER_SIZE;
			if (next_head != rx_tail) {
				rx_buffer[rx_head] = dato;
				rx_head = next_head;
			}

			// Se limpia la bandera de interrupción acumulada durante la lectura para no reingresar por los flancos del mismo byte
			PCIFR |= (1 << PCIF2);
			return;
		}
	}

	// 2. Despertar desde sueño profundo (Power-Down) por pulsador PD2
	// Si el microcontrolador estaba dormido y ocurrió un cambio en el puerto, se solicita el retorno inmediato al estado ACTIVO
	if (durmiendo) {
		contador_segundos_inactividad = 0;
		bandera_despertar_fsm = 1;
		return;
	}

	// 3. Pulsador PD2 (Reset inactividad)
	// Si se presiona el botón de actividad en D2, se reinicia el contador de segundos y se fuerza la vuelta a estado activo
	if (!(estado_pind & (1 << PIND2))) {
		contador_segundos_inactividad = 0;
		bandera_despertar_fsm = 1;
	}

	// 4. Pulsador PD3 (Simular bloqueo de la Prueba B del WDT)
	// Si se presiona el botón de emergencia en D3, se levanta una bandera para que el bucle principal simule un cuelgue
	if (!(estado_pind & (1 << PIND3))) {
		bandera_solicitud_bloqueo = 1;
	}
}
