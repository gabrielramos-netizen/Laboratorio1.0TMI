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
