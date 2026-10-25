// PARTE C - LABORATORIO 4 
#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <stdbool.h>
#include <stdio.h>
#include <xc.h>

#include "UART.h" // libreria proporcionada por los docentes para la comunicación UART

// ASIGNACIÓN DE PINES 
#define LED_VERDE   PD2
#define LED_ROJO    PD3
#define BUZZER      PD4
#define SENSOR_PIR  PD5  // PIR HC-SR501
#define SENSOR_LDR  PC0  // LDR

#define BT_RX_PIN   PB0  // Entrada desde TXD del Bluetooth
#define BT_TX_PIN   PB1  // Salida hacia RXD del Bluetooth

#define SDA_PIN     PC4 //ENTRADAS DE LA LCD
#define SCL_PIN     PC5

#define LCD_ADDR    0x27 // Dirección I2C base del módulo adaptador PCF8574 de la pantalla LCD.

typedef enum { //usando typedef definimos un nuevo tipo de dato llamado 'EstadoPlanta' 
	DESARMADO,
	ARMADO_AUTO,
	ARMADO_MANUAL,
	ALARMA_ON
} EstadoPlanta; //Define los 4 estados posibles en los que puede operar la planta 

EstadoPlanta estado_actual = DESARMADO; //Variable global iniciada en DESARMADO

bool alarma_silenciada = false; //controla si el buzzer y el LED rojo se desactivan temporalmente tras ejecutar 'S'
bool sensor_pir_habilitado = true; // Permite encender o apagar el canal del sensor de movimiento mediante 'P'

// TRANSMISIÓN / RECEPCIÓN BLUETOOTH POR SOFTWARE (PB0/PB1)

 /*Para evitar el conflicto eléctrico con el puerto USB de la PC en los pines PD0/PD1, 
 la transmisión y recepción serial se implementan manualmente sobre los pines PB0 y PB1.*/
 
// Muestrea el bit de entrada en PB0 a 9600 baudios (104 us por bit) realizando validación de ruido
char bluetooth_read_sw(void) {
	if (PINB & (1 << BT_RX_PIN)) {
		return 0; // No hay dato disponible
	}
	
	_delay_us(52);
	if (PINB & (1 << BT_RX_PIN)) return 0; // Falso disparo por ruido
	
	uint8_t dato = 0;
	_delay_us(104);
	
	for (uint8_t i = 0; i < 8; i++) {
		if (PINB & (1 << BT_RX_PIN)) {
			dato |= (1 << i);
		}
		_delay_us(104);
	}
	
	_delay_us(104);
	return (char)dato;
}

/* Genera los pulsos en el pin PB1 simulando un transmisor UART: 
tira la línea a BAJO 104us, conmuta los 8 bits de datos secuencialmente 
y finaliza con un bit de parada en ALTO.*/

void bluetooth_send_sw_byte(char c) {
	// Bit de Inicio (Start Bit = LOW)
	PORTB &= ~(1 << BT_TX_PIN);
	_delay_us(104);
	
	// 8 Bits de Datos (LSB primero)
	for (uint8_t i = 0; i < 8; i++) {
		if (c & (1 << i)) {
			PORTB |= (1 << BT_TX_PIN);
			} else {
			PORTB &= ~(1 << BT_TX_PIN);
		}
		_delay_us(104);
	}
	
	// Bit de Parada (Stop Bit = HIGH)
	PORTB |= (1 << BT_TX_PIN);
	_delay_us(104);
}

/* Envía una cadena de texto a la app vía PB1 y lo replica en la UART nativa por PD1 para la PC */
void bt_print(const char *str) {
	// Enviar a la app vía PB1
	const char *p = str;
	while (*p) {
		bluetooth_send_sw_byte(*p++);
	}
	// Mantener respaldo por la UART Hardware para el Data Visualizer
	uart_print(str);
}

/* Por problemas con las librerias twi_lcd.h y twi.h,, optamos por implentar el codigo directamente aquí*/

// I2C Y LCD

// Configura PC4 (SDA) y PC5 (SCL) como entradas con pull-up para iniciar el bus I2C por software
void i2c_sw_init(void) {
	DDRC &= ~((1 << SDA_PIN) | (1 << SCL_PIN));
	PORTC |= (1 << SDA_PIN) | (1 << SCL_PIN);
}

// Genera la condición de START I2C bajando SDA mientras SCL permanece en alto
void i2c_sw_start(void) {
	DDRC |= (1 << SDA_PIN) | (1 << SCL_PIN);
	PORTC |= (1 << SDA_PIN) | (1 << SCL_PIN);
	_delay_us(10);
	PORTC &= ~(1 << SDA_PIN);
	_delay_us(10);
	PORTC &= ~(1 << SCL_PIN);
	_delay_us(10);
}

// Genera la condición de STOP I2C subiendo SDA mientras SCL está en alto
void i2c_sw_stop(void) {
	DDRC |= (1 << SDA_PIN) | (1 << SCL_PIN);
	PORTC &= ~(1 << SDA_PIN);
	_delay_us(10);
	PORTC |= (1 << SCL_PIN);
	_delay_us(10);
	PORTC |= (1 << SDA_PIN);
	_delay_us(10);
}

// Transmite un byte completo (8 bits) bit por bit mediante software por la línea SDA con pulsos de reloj en SCL
void i2c_sw_write_byte(uint8_t data) {
	DDRC |= (1 << SDA_PIN) | (1 << SCL_PIN);
	for (uint8_t i = 0; i < 8; i++) {
		if (data & 0x80)
		PORTC |= (1 << SDA_PIN);
		else
		PORTC &= ~(1 << SDA_PIN);
		
		_delay_us(10);
		PORTC |= (1 << SCL_PIN);
		_delay_us(10);
		PORTC &= ~(1 << SCL_PIN);
		_delay_us(10);
		data <<= 1;
	}
	DDRC &= ~(1 << SDA_PIN);
	_delay_us(10);
	PORTC |= (1 << SCL_PIN);
	_delay_us(10);
	PORTC &= ~(1 << SCL_PIN);
	_delay_us(10);
	DDRC |= (1 << SDA_PIN);
}

// Divide el dato en 2 nibbles (4 bits cada uno) y los transmite mediante I2C al expandidor PCF8574 de la pantalla LCD
void lcd_i2c_send(uint8_t value, uint8_t mode) {
	uint8_t highnib = value & 0xF0;
	uint8_t lownib = (value << 4) & 0xF0;
	uint8_t backlight = 0x08;

	i2c_sw_start();
	i2c_sw_write_byte((LCD_ADDR << 1) & 0xFE);

	i2c_sw_write_byte(highnib | mode | backlight | 0x04);
	_delay_us(30);
	i2c_sw_write_byte(highnib | mode | backlight);
	_delay_us(30);

	i2c_sw_write_byte(lownib | mode | backlight | 0x04);
	_delay_us(30);
	i2c_sw_write_byte(lownib | mode | backlight);
	_delay_us(30);

	i2c_sw_stop();
}

// Envía un comando de instrucción (RS = 0) a la pantalla LCD
void lcd_comando(uint8_t cmd) {
	lcd_i2c_send(cmd, 0x00);
}

// Envía un carácter de datos (RS = 1) a mostrar en la pantalla LCD
void lcd_data(uint8_t data) {
	lcd_i2c_send(data, 0x01);
}

// Inicializa el bus I2C por software y configura la LCD en modo 4 bits, borra pantalla y activa cursor
void lcd_init(void) {
	i2c_sw_init();
	_delay_ms(50);
	
	lcd_comando(0x33);
	_delay_ms(5);
	lcd_comando(0x32);
	_delay_ms(5);
	lcd_comando(0x28);
	_delay_ms(2);
	lcd_comando(0x0C);
	_delay_ms(2);
	lcd_comando(0x06);
	_delay_ms(2);
	lcd_comando(0x01);
	_delay_ms(5);
}

// Imprime una cadena de caracteres en la pantalla LCD
void lcd_print(const char *str) {
	while (*str) {
		lcd_data(*str++);
	}
}

// Borra todo el contenido visible de la LCD y resetea la posición del cursor
void lcd_clear(void) {
	lcd_comando(0x01);
	_delay_ms(2);
}

// Posiciona el cursor de la pantalla LCD en la columna 'x' y fila 'y'
void lcd_gotoxy(uint8_t x, uint8_t y) {
	uint8_t addr = (y == 0) ? (0x80 + x) : (0xC0 + x);
	lcd_comando(addr);
}

// Limpia el display e imprime la primera línea en la fila 0 y la segunda en la fila 1
void actualizar_pantalla(const char *linea1, const char *linea2) {
	lcd_clear();
	lcd_gotoxy(0, 0);
	lcd_print(linea1);
	lcd_gotoxy(0, 1);
	lcd_print(linea2);
}

// CONFIGURACIÓN DE PINES
void configuracion_pines(void) {
	DDRD |= (1 << LED_VERDE) | (1 << LED_ROJO) | (1 << BUZZER); // Configura LEDs y Buzzer como salidas
	DDRD &= ~(1 << SENSOR_PIR);                                 // Configura pin PD5 como entrada para el PIR
	DDRC &= ~(1 << SENSOR_LDR);                                 // Configura pin PC0 como entrada para el LDR
	
	// Puerto B: PB0 como entrada (RX), PB1 como salida (TX)
	DDRB &= ~(1 << BT_RX_PIN);
	DDRB |= (1 << BT_TX_PIN);
	PORTB |= (1 << BT_TX_PIN); // Estado reposo ALTO para la línea TX
	
	PORTD &= ~((1 << LED_VERDE) | (1 << LED_ROJO) | (1 << BUZZER)); // Inicializa actuadores apagados
}

int main(void) {
	configuracion_pines();
	
	uart_init(103); // Configura la UART hardware a 9600 baudios (16 MHz)
	_delay_ms(20);
	bt_print("ESTADO:DESARMADO\r\n");

	lcd_init();
	actualizar_pantalla("ESTADO:", "DESARMADO");
	
	while (1) {
		// 1. Lectura de comandos en PB0
		char comando = bluetooth_read_sw();
		
		if (comando != 0) {
			if (comando >= 'a' && comando <= 'z') {
				comando -= 32; // Convierte letras minúsculas a mayúsculas
			}
			
			switch (comando) {
				case 'A': // Comando para armar la planta en modo automático
				estado_actual = ARMADO_AUTO;
				alarma_silenciada = false;
				PORTD |= (1 << LED_VERDE);
				PORTD &= ~((1 << LED_ROJO) | (1 << BUZZER));
				bt_print("ESTADO:ARMADO_AUTO\r\n");
				actualizar_pantalla("ESTADO:", "ARMADO AUTO");
				break;
				
				case 'M': // Comando para armar la planta en modo manual
				estado_actual = ARMADO_MANUAL;
				alarma_silenciada = false;
				PORTD |= (1 << LED_VERDE);
				PORTD &= ~((1 << LED_ROJO) | (1 << BUZZER));
				bt_print("ESTADO:ARMADO_MANUAL\r\n");
				actualizar_pantalla("ESTADO:", "ARMADO MANUAL");
				break;
				
				case 'D': // Comando para desarmar el sistema
				estado_actual = DESARMADO;
				alarma_silenciada = false;
				PORTD &= ~((1 << LED_VERDE) | (1 << LED_ROJO) | (1 << BUZZER));
				bt_print("ESTADO:DESARMADO\r\n");
				actualizar_pantalla("ESTADO:", "DESARMADO");
				break;
				
				case 'S': // Comando para silenciar la alarma acústica y luminosa de disparo
				PORTD &= ~((1 << LED_ROJO) | (1 << BUZZER));
				PORTD |= (1 << LED_VERDE);
				alarma_silenciada = true;
				bt_print("ESTADO:SILENCIADO\r\n");
				actualizar_pantalla("ESTADO:", "SILENCIADO");
				break;
				
				case 'P': // TOGGLE DEL PIR (PD5)
				sensor_pir_habilitado = !sensor_pir_habilitado;
				
				// NO alteramos 'alarma_silenciada' para que respete el comando 'S'
				if (sensor_pir_habilitado) {
					bt_print("SENSOR_PIR:ACTIVADO\r\n");
					actualizar_pantalla("PIR: ACTIVADO", "MODO COMPLETO");
					} else {
					bt_print("SENSOR_PIR:DESACTIVADO\r\n");
					actualizar_pantalla("PIR (PD5): OFF", "SOLO MODO LDR");
				}
				
				_delay_ms(1500); // Visualización fija en la LCD por 1.5s
				
				// Restaurar pantalla manteniendo el estado físico real
				if (estado_actual == ARMADO_AUTO) {
					if (alarma_silenciada) {
						PORTD &= ~((1 << LED_ROJO) | (1 << BUZZER));
						PORTD |= (1 << LED_VERDE);
						actualizar_pantalla("ESTADO:", "SILENCIADO");
						} else {
						PORTD |= (1 << LED_VERDE);
						PORTD &= ~((1 << LED_ROJO) | (1 << BUZZER));
						actualizar_pantalla("ESTADO:", "ARMADO AUTO");
					}
					} else if (estado_actual == ALARMA_ON) {
					if (alarma_silenciada) {
						PORTD &= ~((1 << LED_ROJO) | (1 << BUZZER));
						PORTD |= (1 << LED_VERDE);
						actualizar_pantalla("ESTADO:", "SILENCIADO");
						} else {
						actualizar_pantalla("ALARMA ACTIVA!", "ZONA: MOVIMIENTO");
					}
					} else if (estado_actual == DESARMADO) {
					actualizar_pantalla("ESTADO:", "DESARMADO");
					} else if (estado_actual == ARMADO_MANUAL) {
					actualizar_pantalla("ESTADO:", "ARMADO MANUAL");
				}
				break;
				
				case 'F': // FORZAR ALARMA EN MODO MANUAL
				if (estado_actual == ARMADO_MANUAL) {
					PORTD &= ~(1 << LED_VERDE);
					PORTD |= (1 << LED_ROJO) | (1 << BUZZER);
					estado_actual = ALARMA_ON;
					alarma_silenciada = false;
					bt_print("ALARMA:ON;ZONA:FORZADO\r\n");
					actualizar_pantalla("ALARMA ACTIVA!", "ZONA: FORZADO");
				}
				break;
				
				case 'R': // Comando de reset general para desarmar y apagar todo
				PORTD &= ~((1 << LED_VERDE) | (1 << LED_ROJO) | (1 << BUZZER));
				estado_actual = DESARMADO;
				alarma_silenciada = false;
				bt_print("ESTADO:DESARMADO\r\n");
				actualizar_pantalla("ESTADO:", "DESARMADO");
				break;
			}
		}
		
		// 2. Máquina de Estados Continua
		switch (estado_actual) {
			case DESARMADO:
			PORTD &= ~((1 << LED_VERDE) | (1 << LED_ROJO) | (1 << BUZZER));
			break;
			
			case ARMADO_AUTO:
			// Monitorea el PIR en PD5 si está habilitado
			if (sensor_pir_habilitado && (PIND & (1 << SENSOR_PIR))) {
				if (!alarma_silenciada || estado_actual != ALARMA_ON) {
					PORTD &= ~(1 << LED_VERDE);
					PORTD |= (1 << LED_ROJO) | (1 << BUZZER);
					
					bt_print("ALARMA:ON;ZONA:MOVIMIENTO\r\n");
					actualizar_pantalla("ALARMA ACTIVA!", "ZONA: MOVIMIENTO");
					estado_actual = ALARMA_ON;
					alarma_silenciada = false;
				}
			}
			// Monitorea el LDR en PC0
			else if (PINC & (1 << SENSOR_LDR)) {
				if (!alarma_silenciada || estado_actual != ALARMA_ON) {
					PORTD &= ~(1 << LED_VERDE);
					PORTD |= (1 << LED_ROJO) | (1 << BUZZER);
					
					bt_print("ALARMA:ON;ZONA:VENTANA\r\n");
					actualizar_pantalla("ALARMA ACTIVA!", "ZONA: VENTANA");
					estado_actual = ALARMA_ON;
					alarma_silenciada = false;
				}
			}
			else if (!alarma_silenciada && estado_actual == ARMADO_AUTO) {
				PORTD |= (1 << LED_VERDE);
				PORTD &= ~((1 << LED_ROJO) | (1 << BUZZER));
			}
			break;
			
			case ARMADO_MANUAL:
			if (estado_actual != ALARMA_ON) {
				PORTD |= (1 << LED_VERDE);
				PORTD &= ~((1 << LED_ROJO) | (1 << BUZZER));
			}
			break;
			
			case ALARMA_ON:
			if (!alarma_silenciada) {
				PORTD &= ~(1 << LED_VERDE);
				PORTD |= (1 << LED_ROJO) | (1 << BUZZER);
				} else {
				PORTD |= (1 << LED_VERDE);
				PORTD &= ~((1 << LED_ROJO) | (1 << BUZZER));
			}
			break;
		}
	}
	
	return 0;
}
