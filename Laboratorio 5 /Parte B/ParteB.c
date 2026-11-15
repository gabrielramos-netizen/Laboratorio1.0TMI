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

// TIMER 1
// Aca se incrementa el contador global de inactividad cada 1 segundo exacto
ISR(TIMER1_COMPA_vect) {
	contador_segundos_inactividad++;
}

// Aca se configura el Timer 1 de 16 bits en modo CTC para generar una base de tiempo precisa de 1 segundo
void Timer1_Init(void) {
	TCCR1A = 0;
	// Se selecciona el modo CTC (WGM12) y un prescaler de 1024 (CS12 y CS10)
	TCCR1B = (1 << WGM12) | (1 << CS12) | (1 << CS10);
	// Se carga 15624 en OCR1A porque (16 MHz / 1024) - 1 = 15624 ciclos para completar 1 segundo
	OCR1A = 15624;
	TIMSK1 = (1 << OCIE1A);
}

// Aca se lee de forma atómica la variable de 32 bits de inactividad para evitar lecturas corruptas si ocurre una interrupción a mitad de camino
static uint32_t leer_inactividad(void) {
	uint32_t v;
	uint8_t sreg = SREG;
	cli();
	v = contador_segundos_inactividad;
	SREG = sreg;
	return v;
}

// ADC
// Aca se prepara el conversor analógico-digital para leer el voltaje del potenciómetro conectado en A1
void ADC_Init(void) {
	// Se configura el pin PC1 como entrada sin resistencia pull-up interna para no alterar el divisor de tensión del potenciómetro
	DDRC  &= ~(1 << PORTC1);
	PORTC &= ~(1 << PORTC1);
	// Se deshabilita el búfer de entrada digital en ADC1 para reducir ruido y consumo parásito
	DIDR0 |= (1 << ADC1D);
	// Se elige AVCC (5 V) como tensión de referencia
	ADMUX  = (1 << REFS0);
	// Se enciende el ADC y se fija un prescaler de 128 para que el reloj del conversor opere a 125 kHz dentro del rango óptimo
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

// Aca se realiza la adquisición analógica filtrando el ruido mediante promediado de muestras
uint16_t ADC_Read(uint8_t canal) {
	ADMUX = (ADMUX & 0xF0) | (canal & 0x07);
	// Se ejecuta y descarta una primera conversión porque el primer muestreo tras encender el ADC suele ser impreciso
	ADCSRA |= (1 << ADSC);
	while (ADCSRA & (1 << ADSC));

	// Se toman 8 lecturas consecutivas y se dividen entre 8 desplazando 3 bits para estabilizar el valor mostrado
	uint32_t suma = 0;
	for (uint8_t i = 0; i < 8; i++) {
		ADCSRA |= (1 << ADSC);
		while (ADCSRA & (1 << ADSC));
		suma += ADC;
	}
	return (uint16_t)(suma >> 3);
}

// CONFIGURACIÓN WDT
// Aca se configura el Watchdog en modo interrupción pura cada 8 segundos para despertar el micro en la Prueba A sin reiniciarlo
void WDT_Config_Despertador_8s(void) {
	cli();
	wdt_reset();
	// Se sigue la secuencia cronometrada exigida por el fabricante habilitando WDCE y WDE antes de modificar WDTCSR
	WDTCSR |= (1 << WDCE) | (1 << WDE);
	// Se activa únicamente WDIE (interrupción) con los bits WDP3 y WDP0 para obtener el intervalo máximo de 8 segundos
	WDTCSR = (1 << WDIE) | (1 << WDP3) | (1 << WDP0);
	sei();
}

// Aca se configura el Watchdog en modo reinicio por hardware cada 2 segundos para proteger el sistema en la Prueba B
void WDT_Config_Reset_2s(void) {
	cli();
	wdt_reset();
	WDTCSR |= (1 << WDCE) | (1 << WDE);
	// Se activa únicamente WDE (reset) con los bits WDP2 y WDP1 para fijar un tiempo límite de 2 segundos
	WDTCSR = (1 << WDE) | (1 << WDP2) | (1 << WDP1);
	sei();
}

// PROTOCOLO DE TRAMAS
// Aca se empaqueta y envía por Bluetooth una trama de 5 bytes con delimitadores y suma de verificación XOR
void Enviar_Trama_BT(uint8_t cmd, uint8_t dato) {
	uint8_t chk = cmd ^ dato;
	// Se añade una pequeña pausa de 5 ms para darle tiempo al receptor remoto de procesar tramas consecutivas
	_delay_ms(5);
	SoftUART_Transmit(HEADER_BYTE);
	SoftUART_Transmit(cmd);
	SoftUART_Transmit(dato);
	SoftUART_Transmit(chk);
	SoftUART_Transmit(FOOTER_BYTE);
}

// Aca se extraen y validan las tramas almacenadas en el búfer circular fuera de las interrupciones para no bloquear el sistema
void Procesar_Tramas(void) {
	while (rx_head != rx_tail) {
		// Si el byte actual no corresponde a la cabecera 0xAA, se descarta para resincronizar el inicio de trama
		if (rx_buffer[rx_tail] != HEADER_BYTE) {
			rx_tail = (rx_tail + 1) % BUFFER_SIZE;
			continue;
		}

		// Se calcula cuántos bytes hay acumulados en el búfer circular
		uint8_t bytes_disponibles = (rx_head >= rx_tail)
		? (rx_head - rx_tail)
		: (BUFFER_SIZE - rx_tail + rx_head);

		// Si todavía no llegaron los 5 bytes completos de la trama, se sale y se espera en la siguiente iteración sin bloquear
		if (bytes_disponibles < 5) {
			return;
		}

		recibiendo_trama = 0;

		uint8_t cmd  = rx_buffer[(rx_tail + 1) % BUFFER_SIZE];
		uint8_t dato = rx_buffer[(rx_tail + 2) % BUFFER_SIZE];
		uint8_t chk  = rx_buffer[(rx_tail + 3) % BUFFER_SIZE];
		uint8_t stop = rx_buffer[(rx_tail + 4) % BUFFER_SIZE];

		// Se comprueba que el byte de parada sea 0x55 y que el checksum XOR coincida antes de ejecutar cualquier acción
		if (stop == FOOTER_BYTE && chk == (cmd ^ dato)) {
			if (cmd == 0x01) {
				// Comando 0x01: devuelve el código del estado actual de la FSM (0=ACTIVO, 1=STANDBY, 2=APAGADO)
				Enviar_Trama_BT(0x81, codigo_estado_actual);
				} else if (cmd == 0x02) {
				// Comando 0x02: enciende temporalmente el ADC si estaba apagado por PRR, lee el potenciómetro y responde en 8 bits
				uint8_t prr_previo = PRR;
				PRR &= ~(1 << PRADC);
				ADCSRA |= (1 << ADEN);
				_delay_us(150);
				uint16_t lectura = ADC_Read(POT_CANAL_ADC);
				// Si el ADC estaba deshabilitado antes de la consulta, se vuelve a apagar para conservar el ahorro energético
				if (prr_previo & (1 << PRADC)) {
					ADCSRA &= ~(1 << ADEN);
					PRR |= (1 << PRADC);
				}
				Enviar_Trama_BT(0x82, (uint8_t)(lectura >> 2));
				} else if (cmd == 0x03) {
				// Comando 0x03: reinicia el contador de inactividad manteniendo el sistema despierto
				contador_segundos_inactividad = 0;
				bandera_despertar_fsm = 1;
				Enviar_Trama_BT(0x83, 0x01);
				} else if (cmd == 0x04) {
				// Comando 0x04: permite conmutar de forma controlada entre los estados permitidos ACTIVO y STANDBY
				if (dato == 0x00) {
					bandera_despertar_fsm = 0;
					contador_segundos_inactividad = 0;
					codigo_estado_actual = 0;
					estado_actual = st_activo;
					} else if (dato == 0x01) {
					bandera_despertar_fsm = 0;
					contador_segundos_inactividad = 10;
					codigo_estado_actual = 1;
					estado_actual = st_standby;
				}
				Enviar_Trama_BT(0x83, 0x01);
				} else if (cmd == 0x05) {
				// Comando 0x05: enciende o apaga el LED indicador del pin D13 y renueva la actividad
				contador_segundos_inactividad = 0;
				bandera_despertar_fsm = 1;
				if (dato == 0x01) PORTB |= (1 << PORTB5);
				else PORTB &= ~(1 << PORTB5);
				Enviar_Trama_BT(0x83, 0x01);
				} else if (cmd == 0xFF) {
				// Comando 0xFF: activa la simulación de fallo crítico para probar la recuperación por Watchdog
				bandera_solicitud_bloqueo = 1;
				} else {
				// Si el código de comando no existe, se responde con el código de error 0xFE
				Enviar_Trama_BT(0x83, 0xFE);
			}
			// Se avanza el índice de lectura 5 posiciones tras procesar la trama válida
			rx_tail = (rx_tail + 5) % BUFFER_SIZE;
			} else {
			// Si el checksum o el byte de fin fallan, se reporta error y se avanza un byte para buscar la siguiente cabecera válida
			Enviar_Trama_BT(0x83, 0xFE);
			rx_tail = (rx_tail + 1) % BUFFER_SIZE;
		}
	}
}

// RESTAURAR PERIFÉRICOS
// Aca se envía el bit P3 en alto al expansor PCF8574 para encender la luz de fondo de la pantalla LCD
void Encender_Luz_LCD(void) {
	twi_start();
	twi_write_cmd((PCF8574 << 1) | 0);
	twi_write_dwr(0x08);
	twi_stop();
	_delay_ms(5);
}

// Aca se reactivan los relojes de ADC y TWI en el registro PRR y se reinicializa la LCD tras salir del modo Power-Down
void Restaurar_Perifericos_I2C(void) {
	PRR &= ~((1 << PRADC) | (1 << PRTWI));
	ADCSRA |= (1 << ADEN);
	_delay_ms(20);
	twi_init();
	twi_lcd_init();
	_delay_ms(30);
	Encender_Luz_LCD();
	ultimo_estado_lcd = 0xFF;
}

// ESTADOS DE LA FSM
// Aca se implementa el estado ACTIVO donde operan todas las tareas periódicas y periféricos requeridos
void st_activo(void) {
	codigo_estado_actual = 0;
	// Se mantiene el WDT en modo reinicio de 2 segundos para supervisar que el bucle principal no se trabe
	WDT_Config_Reset_2s();

	// Se dejan encendidos ADC y TWI, mientras se apagan USART0, SPI, Timer0 y Timer2 mediante PRR para ahorrar corriente
	PRR &= ~((1 << PRADC) | (1 << PRTWI));
	PRR |= (1 << PRUSART0) | (1 << PRSPI) | (1 << PRTIM2) | (1 << PRTIM0);

	// Se actualiza la primera línea de la pantalla solo al ingresar a este estado para evitar reescrituras innecesarias
	if (ultimo_estado_lcd != 0) {
		Encender_Luz_LCD();
		twi_lcd_clear();
		_delay_ms(15);
		twi_lcd_cmd(0x80);
		_delay_ms(5);
		twi_lcd_msg("Estado: ACTIVO  ");
		ultimo_estado_lcd = 0;
	}

	// Se refresca la segunda línea cada 200 ms con el valor del potenciómetro y el último byte recibido por Bluetooth
	static uint16_t contador_refresco = 0;
	if (++contador_refresco >= 200) {
		contador_refresco = 0;
		char linea2[17];
		uint8_t pot8 = (uint8_t)(ADC_Read(POT_CANAL_ADC) >> 2);
		snprintf(linea2, sizeof(linea2), "POT:%3u RX:%02X   ", pot8, ultimo_byte_rx);
		twi_lcd_cmd(0xC0);
		twi_lcd_msg(linea2);
	}
	_delay_ms(1);

	// Se enciende el LED de D13 como indicador visual de operación activa
	PORTB |= (1 << PORTB5);
}

// Aca se implementa el estado STANDBY tras 10 segundos de inactividad usando SLEEP_MODE_IDLE
void st_standby(void) {
	codigo_estado_actual = 1;
	// Se apaga el LED de D13 para reducir el consumo de corriente
	PORTB &= ~(1 << PORTB5);
	WDT_Config_Reset_2s();

	// Se apaga también el ADC mediante PRR porque en reposo no se realizan lecturas periódicas del potenciómetro
	PRR |= (1 << PRADC) | (1 << PRUSART0) | (1 << PRSPI) | (1 << PRTIM2) | (1 << PRTIM0);

	if (ultimo_estado_lcd != 1) {
		Encender_Luz_LCD();
		twi_lcd_clear();
		_delay_ms(15);
		twi_lcd_cmd(0x80);
		_delay_ms(5);
		twi_lcd_msg("Esperando...    ");
		twi_lcd_cmd(0xC0);
		twi_lcd_msg("Modo Standby    ");
		ultimo_estado_lcd = 1;
	}

	// Si hay una trama Bluetooth entrando o pendiente en el búfer, se evita dormir la CPU hasta terminar de procesarla
	if (recibiendo_trama || (rx_head != rx_tail)) {
		return;
	}

	// Se configura el modo IDLE que detiene la CPU pero mantiene vivos el Timer 1 y las interrupciones PCINT2
	set_sleep_mode(SLEEP_MODE_IDLE);
	cli();
	if (!bandera_despertar_fsm && !bandera_solicitud_bloqueo && !recibiendo_trama) {
		sleep_enable();
		sei();
		sleep_cpu();
		sleep_disable();
	}
	sei();
}

// Aca se implementa el estado APAGADO tras 30 segundos de inactividad usando SLEEP_MODE_PWR_DOWN
void st_apagado(void) {
	codigo_estado_actual = 2;
	PORTB &= ~(1 << PORTB5);

	// Se notifica por Bluetooth a la aplicación móvil que el sistema pasa a APAGADO antes de deshabilitar las comunicaciones
	Enviar_Trama_BT(0x81, 0x02);
	_delay_ms(20);

	// Se borra el texto y se escribe 0x00 en el PCF8574 para apagar por completo la retroiluminación de la pantalla LCD
	twi_lcd_clear();
	_delay_ms(15);
	twi_start();
	twi_write_cmd((PCF8574 << 1) | 0);
	twi_write_dwr(0x00);
	twi_stop();
	ultimo_estado_lcd = 2;

	// Se apaga primero el bit ADEN antes de cortar el reloj del ADC en PRR, ya que de lo contrario el conversor sigue consumiendo energía
	ADCSRA &= ~(1 << ADEN);
	PRR |= (1 << PRADC) | (1 << PRUSART0) | (1 << PRSPI) | (1 << PRTIM2) | (1 << PRTIM0);

	// Se deshabilita la interrupción de recepción serie PCINT20 porque en Power-Down no se pueden recibir comandos UART convencionales
	PCMSK2 &= ~(1 << PCINT20);

	// Se configura INT0 por nivel bajo en D2 como única fuente física de despertar junto con el WDT de 8 segundos
	EICRA &= ~((1 << ISC01) | (1 << ISC00));
	WDT_Config_Despertador_8s();
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);

	// Se permanece en sueño profundo hasta que el usuario presione D2 o ocurra una solicitud de bloqueo
	while (codigo_estado_actual == 2 && !bandera_despertar_fsm && !bandera_solicitud_bloqueo) {
		cli();
		EIFR  |= (1 << INTF0);
		PCIFR |= (1 << PCIF2);
		EIMSK |= (1 << INT0);
		durmiendo = 1;
		sleep_enable();
		sei();
		sleep_cpu();
		sleep_disable();
		durmiendo = 0;

		// Si el microcontrolador despertó por la interrupción periódica de 8 s del WDT (Prueba A), realiza una lectura rápida y vuelve a dormir
		if (bandera_wdt_despertador) {
			bandera_wdt_despertador = 0;
			contador_despertates_wdt++;
			// Se reactiva el ADC brevemente para medir el sensor sin encender la pantalla ni salir del modo de ahorro
			PRR &= ~(1 << PRADC);
			ADCSRA |= (1 << ADEN);
			_delay_us(150);
			ultima_lectura_adc_wdt = ADC_Read(POT_CANAL_ADC);
			// Se vuelve a apagar el ADC antes de regresar al ciclo de sueño profundo
			ADCSRA &= ~(1 << ADEN);
			PRR |= (1 << PRADC);
		}
	}

	// Al salir definitivamente de APAGADO, se desactiva INT0 y se vuelve a habilitar la recepción Bluetooth en D4
	EIMSK &= ~(1 << INT0);
	PCMSK2 |= (1 << PCINT20);
	rx_head = 0;
	rx_tail = 0;
	recibiendo_trama = 0;

	// Se restablecen el LED, el WDT de 2 segundos y los periféricos I2C de la pantalla LCD
	PORTB |= (1 << PORTB5);
	WDT_Config_Reset_2s();
	Restaurar_Perifericos_I2C();

	contador_segundos_inactividad = 0;
	codigo_estado_actual = 0;
	estado_actual = st_activo;

	// Se avisa a la aplicación móvil que el sistema recuperó el estado ACTIVO
	Enviar_Trama_BT(0x81, 0x00);
}

// PROGRAMA PRINCIPAL
int main(void) {
	// Se configura D13 (PB5) como salida para el LED y D2/D3 como entradas con pull-up interno para los pulsadores físicos
	DDRB |= (1 << PORTB5);
	DDRD &= ~((1 << PORTD2) | (1 << PORTD3));
	PORTD |= (1 << PORTD2) | (1 << PORTD3);

	// Se inicializan todos los módulos de comunicación, temporización, conversión analógica y pantalla LCD
	SoftUART_Init();
	Timer1_Init();
	ADC_Init();
	twi_init();
	twi_lcd_init();
	_delay_ms(50);

	// Se consulta la memoria EEPROM para verificar si el arranque actual proviene de un reinicio forzado por el Watchdog (Prueba B)
	uint8_t flag_eeprom = eeprom_read_byte(EEPROM_WDT_FLAG_ADDR);
	if (flag_eeprom == EEPROM_MAGIC_BYTE) {
		// Se limpia la bandera mágica para que futuros arranques normales no muestren el aviso de fallo
		eeprom_write_byte(EEPROM_WDT_FLAG_ADDR, 0x00);
		Encender_Luz_LCD();
		twi_lcd_clear();
		_delay_ms(30);
		twi_lcd_cmd(0x80);
		_delay_ms(10);
		twi_lcd_msg("RESET POR WDT!  ");
		for (uint8_t i = 0; i < 3; i++) {
			_delay_ms(1000);
		}
		ultimo_estado_lcd = 0xFF;
	}

	// Se activa la supervisión del WDT a 2 segundos, se habilitan las interrupciones globales y se inicia en el estado ACTIVO
	WDT_Config_Reset_2s();
	sei();
	estado_actual = st_activo;

	while (1) {
		// Si se activó la Prueba B (por pulsador D3 o comando 0xFF), se guarda la marca en EEPROM y se fuerza un bucle infinito sin refrescar el WDT
		if (bandera_solicitud_bloqueo) {
			cli();
			eeprom_write_byte(EEPROM_WDT_FLAG_ADDR, EEPROM_MAGIC_BYTE);
			PRR &= ~((1 << PRTWI));
			twi_init();
			twi_lcd_init();
			_delay_ms(30);
			Encender_Luz_LCD();
			twi_lcd_clear();
			_delay_ms(30);
			twi_lcd_cmd(0x80);
			_delay_ms(10);
			twi_lcd_msg("SISTEMA TRABADO ");
			WDT_Config_Reset_2s();
			// Al quedar atrapado aquí sin ejecutar wdt_reset(), el temporizador provoca un reinicio por hardware a los 2 segundos
			while (1);
		}

		// Se procesan las tramas Bluetooth pendientes siempre que el sistema no esté en el estado APAGADO
		if (codigo_estado_actual != 2) {
			Procesar_Tramas();
		}

		// Si ocurrió un evento válido de despertar, se restablece el tiempo de inactividad y se retorna inmediatamente a ACTIVO
		if (bandera_despertar_fsm) {
			cli();
			bandera_despertar_fsm = 0;
			contador_segundos_inactividad = 0;
			codigo_estado_actual = 0;
			estado_actual = st_activo;
			sei();
		}

		// Se reinicia el contador del Watchdog en cada iteración normal para indicar que el programa funciona sin bloqueos
		wdt_reset();

		// Se evalúan los umbrales temporales de inactividad para seleccionar el puntero a función del estado correspondiente sin usar switch-case
		uint32_t t = leer_inactividad();
		if (t < 10) {
			estado_actual = st_activo;
			} else if (t < 30) {
			estado_actual = st_standby;
			} else {
			estado_actual = st_apagado;
		}

		// Se ejecuta la función correspondiente al estado actual de la máquina de estados
		estado_actual();
	}
}
