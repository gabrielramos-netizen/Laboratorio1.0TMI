// PARTE A - LAB 3 | TMI
#define F_CPU 16000000UL // Frecuencia del UC (16 MHz) - Debe ir ANTES de util/delay.h

#include <avr/io.h>
#include <stdio.h>
#include <stdbool.h>
#include <util/delay.h>

#define DHT11_MAX_TEMP 50 // Temperatura maxima que percibe el sensor DHT11
#define DHT11_PIN PD2     // Pin de datos del sensor DHT11

// Dirección I2C del adaptador PCF8574 (0x27 desplazado 1 bit a la izquierda para el bus AVR)
#define I2C_LCD_ADDR (0x27 << 1)

typedef struct {
	int16_t pm_min; // Inicio de zona ideal
	int16_t pm_max; // Fin de zona ideal
} UmbralesTemperatura;

UmbralesTemperatura sistema_umbrales = {16, 25}; // pm_min = 16, pm_max = 25 --> para el punto medio

// Función para intentar actualizar el punto medio
bool actualizar_punto_medio(int16_t nuevo_min) {
	int16_t nuevo_max = nuevo_min + 9; // Ancho de 10°C (ej. 16 a 25)
	
	// El limite absoluto para que el Rango 5 (Alta velocidad) sea medible por el DHT11
	// es que el limite superior del punto medio (nuevo_max) no exceda 29°C.
	// Asi, Rango 3 (Baja) llega a 39°C, Rango 4 (Media) llega a 49°C, y Rango 5 arranca en 50°C.
	if ((nuevo_max + 21) > (DHT11_MAX_TEMP + 21)) {
		return false;
	}
	
	// Forma simplificada y exacta: el maximo punto medio permitido es 29°C
	if (nuevo_max > 29) {
		return false; // Bloquea si supera la escala de seguridad del DHT11
	}

	sistema_umbrales.pm_min = nuevo_min;
	sistema_umbrales.pm_max = nuevo_max;
	return true;
}

// Definimos una funcion para la comunicacion serial

void USART_init(unsigned int baud) {
	unsigned int ubrr = F_CPU/16/baud - 1; // ubrr = F_CPU/16*BAUDRATE - 1

	// Configuramos la tasa de baudios usando la parte baja y alta del registro UBRR0
	UBRR0H = (unsigned char)(ubrr >> 8);
	UBRR0L = (unsigned char)ubrr;
	
	// Habilitamos la transmision y recepcion en los pines TX Y RX
	UCSR0B = (1 << RXEN0) | (1 << TXEN0);

	// Se tiene 8 bits de datos, 1 bit de parada, y sin paridad
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void USART_transmit(unsigned char data){
	// Esperamos a que el buffer de transmision esté vacío
	while(!(UCSR0A & (1 << UDRE0))); // UCSR0A bit UDRE0 en 1 indica que se puede transmitir nuevo dato
	// Colocamos el dato en el registro para enviarlo
	UDR0 = data;
}

unsigned char USART_receive(void) {
	// Espera a que los datos sean recibidos
	while(!(UCSR0A & (1 << RXC0)));
	// Retorna el dato leido
	return UDR0;
}

bool USART_available(void) {
	return (UCSR0A & (1 << RXC0)); // Retorna verdadero si hay un byte listo para leer
}

void USART_sendString(const char* str){
	while(*str){ // Cuando encuentre \0 se detiene el bucle
		USART_transmit(*str++); // Incrementa el puntero en 1
	}
}

// MÓDULO COMUNICACIÓN I2C (TWI) Y PANTALLA LCD
void I2C_init(void) {
	TWSR = 0x00; // Prescaler = 1
	TWBR = 72;   // Frecuencia SCL ≈ 100 kHz (16 MHz)
}

void I2C_start(void) {
	TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
	while (!(TWCR & (1 << TWINT)));
}

void I2C_stop(void) {
	TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}

void I2C_write(uint8_t data) {
	TWDR = data;
	TWCR = (1 << TWINT) | (1 << TWEN);
	while (!(TWCR & (1 << TWINT)));
}

#define LCD_BACKLIGHT 0x08
#define ENABLE 0x04
#define RS_BIT 0x01

void lcd_send_i2c(uint8_t val) {
	I2C_start();
	I2C_write(I2C_LCD_ADDR);
	I2C_write(val | LCD_BACKLIGHT);
	I2C_stop();
}

void lcd_pulse_enable_i2c(uint8_t val) {
	lcd_send_i2c(val | ENABLE);
	_delay_us(1);
	lcd_send_i2c(val & ~ENABLE);
	_delay_us(50);
}

void lcd_send_nibble_i2c(uint8_t nibble, uint8_t mode) {
	uint8_t data = (nibble & 0xF0) | mode;
	lcd_send_i2c(data);
	lcd_pulse_enable_i2c(data);
}

void lcd_command(uint8_t cmd) {
	lcd_send_nibble_i2c(cmd & 0xF0, 0);
	lcd_send_nibble_i2c((cmd << 4) & 0xF0, 0);
	_delay_ms(2);
}

void lcd_char(char data) {
	lcd_send_nibble_i2c(data & 0xF0, RS_BIT);
	lcd_send_nibble_i2c((data << 4) & 0xF0, RS_BIT);
	_delay_us(100);
}

void lcd_init(void) {
	I2C_init();
	_delay_ms(50);

	// Secuencia de inicializacion en modo 4 bits via I2C
	lcd_send_nibble_i2c(0x30, 0);
	_delay_ms(5);
	lcd_send_nibble_i2c(0x30, 0);
	_delay_us(200);
	lcd_send_nibble_i2c(0x30, 0);
	lcd_send_nibble_i2c(0x20, 0);

	lcd_command(0x28); // 2 lineas, matriz 5x7
	lcd_command(0x0C); // Encender pantalla, apagar cursor
	lcd_command(0x01); // Limpiar pantalla
	_delay_ms(2);
}

void lcd_set_cursor(uint8_t row, uint8_t col) {
	uint8_t pos = (row == 0) ? (0x80 + col) : (0xC0 + col);
	lcd_command(pos);
}

void lcd_send_string(const char *str) {
	while (*str) {
		lcd_char(*str++);
	}
}

// Enviar pulso de inicio al DHT11
void dht11_start(void) {
	DDRD |= (1 << DHT11_PIN);    // Configurar PD2 como SALIDA
	PORTD &= ~(1 << DHT11_PIN);  // Poner PD2 en BAJO
	_delay_ms(18);               // Mantenerlo en BAJO por al menos 18 ms
	
	PORTD |= (1 << DHT11_PIN);   // Poner PD2 en ALTO
	_delay_us(30);               // Esperar 30 us
	DDRD &= ~(1 << DHT11_PIN);   // Cambiar PD2 a ENTRADA para escuchar
}

// 2. Verificar la respuesta del sensor
uint8_t dht11_check_response(void) {
	uint8_t response = 0;
	_delay_us(40);
	
	// El DHT11 debe responder poniendo la línea en BAJO
	if (!(PIND & (1 << DHT11_PIN))) {
		_delay_us(80);
		// Luego el DHT11 debe poner la línea en ALTO por 80 us
		if (PIND & (1 << DHT11_PIN)) response = 1;
		_delay_us(50);
	}
	return response;
}

// 3. Leer un byte (8 bits) enviado por el DHT11
uint8_t dht11_read_byte(void) {
	uint8_t i, data = 0;
	for (i = 0; i < 8; i++) {
		// Espera a que la línea pase a ALTO (inicio de la transmisión del bit)
		while (!(PIND & (1 << DHT11_PIN)));
		
		_delay_us(30); // Esperar 30 us para evaluar el ancho del pulso alto
		
		if (PIND & (1 << DHT11_PIN)) {
			// Si pasados 30 us la línea sigue en ALTO, es un '1'
			data = (data << 1) | 0x01;
			} else {
			// Si ya volvió a BAJO antes de los 30 us, es un '0'
			data = (data << 1);
		}
		
		// Espera a que termine el pulso ALTO restante antes del siguiente bit
		while (PIND & (1 << DHT11_PIN));
	}
	return data;
}

// 4. Función principal de lectura del DHT11
uint8_t dht11_read_data(uint8_t *humedad, uint8_t *temperatura) {
	uint8_t h_int, h_dec, t_int, t_dec, checksum;
	
	dht11_start();
	
	if (dht11_check_response()) {
		h_int    = dht11_read_byte(); // Byte 1: Humedad Entera
		h_dec    = dht11_read_byte(); // Byte 2: Humedad Decimal
		t_int    = dht11_read_byte(); // Byte 3: Temperatura Entera
		t_dec    = dht11_read_byte(); // Byte 4: Temperatura Decimal
		checksum = dht11_read_byte(); // Byte 5: Checksum
		
		// Validar integridad de los datos mediante el checksum
		if ((h_int + h_dec + t_int + t_dec) == checksum) {
			*humedad = h_int;
			*temperatura = t_int;
			return 1; // Lectura correcta
		}
	}
	return 0; // Error de lectura o checksum incorrecto
}

// Utilizar punteros hace mas sencillo modificar los valores del calefactor y ventilador
void evaluar_estado(int16_t temp_actual, uint8_t *calefactor, uint8_t *pwm_ventilador) {
	if (temp_actual < sistema_umbrales.pm_min) {
		// Rango 1: Calefactor encendido, Ventilador apagado
		*calefactor = 1;
		*pwm_ventilador = 0;
	}
	else if (temp_actual <= sistema_umbrales.pm_max) {
		// Rango 2: Punto Medio (Ambos apagados)
		*calefactor = 0;
		*pwm_ventilador = 0;
	}
	else if (temp_actual <= (sistema_umbrales.pm_max + 10)) {
		// Rango 3: Ventilador Baja Velocidad (~33% Duty Cycle)
		*calefactor = 0;
		*pwm_ventilador = 85; // 85 / 255 ≈ 33%
	}
	else if (temp_actual <= (sistema_umbrales.pm_max + 20)) {
		// Rango 4: Ventilador Media Velocidad (~66% Duty Cycle)
		*calefactor = 0;
		*pwm_ventilador = 170; // 170 / 255 ≈ 66%
	}
	else {
		// Rango 5: Ventilador Alta Velocidad (100% Duty Cycle)
		*calefactor = 0;
		*pwm_ventilador = 255;
	}
}

void hardware_init(void) {
	// 1. Configurar PB0 como salida digital para el Calefactor
	DDRB |= (1 << PORTB0);
	PORTB &= ~(1 << PORTB0); // Inicia apagado

	// 2. Configurar PD6 (OC0A) como salida digital para el PWM del Ventilador
	DDRD |= (1 << PORTD6);

	// 3. Configurar Timer0 en modo Fast PWM de 8 bits
	TCCR0A = (1 << WGM01) | (1 << WGM00) | (1 << COM0A1);

	// Configurar Prescaler en 64 (CS01 = 1, CS00 = 1)
	TCCR0B = (1 << CS01) | (1 << CS00);

	// Iniciar con PWM en 0 (Ventilador apagado)
	OCR0A = 0;
}

void aplicar_actuadores(uint8_t calefactor, uint8_t pwm_ventilador) {
	// Control del Calefactor (PB0)
	if (calefactor) {
		PORTB |= (1 << PORTB0);  // Enciende calefactor
		} else {
		PORTB &= ~(1 << PORTB0); // Apaga calefactor
	}

	// Control de la velocidad del Ventilador (Carga el Duty Cycle en OCR0A)
	OCR0A = pwm_ventilador;
}

int main(void)
{
	uint8_t humedad = 0, temperatura = 0;
	uint8_t calefactor = 0, pwm_ventilador = 0;
	char buffer_lcd[17];
	char buffer_usart[64];

	hardware_init();
	USART_init(9600);
	lcd_init();

	lcd_set_cursor(0, 0);
	lcd_send_string("Sistema Control");
	lcd_set_cursor(1, 0);
	lcd_send_string("Iniciando...");
	_delay_ms(1500); // Retardo para estabilizar el sensor
	lcd_command(0x01);

	while(1)
	{
		// 1. Lectura del sensor DHT11
		if (dht11_read_data(&humedad, &temperatura)) {
			// 2. Evaluar estado de actuadores según el punto medio actual
			evaluar_estado(temperatura, &calefactor, &pwm_ventilador);
			aplicar_actuadores(calefactor, pwm_ventilador);

			// 3. Actualización de Pantalla LCD 16x2
			lcd_set_cursor(0, 0);
			sprintf(buffer_lcd, "Temp:%2d C  H:%2d%%", temperatura, humedad);
			lcd_send_string(buffer_lcd);

			lcd_set_cursor(1, 0);
			sprintf(buffer_lcd, "PM: %2d-%2d C", sistema_umbrales.pm_min, sistema_umbrales.pm_max);
			lcd_send_string(buffer_lcd);

			sprintf(buffer_usart, "TEMP:%d,CAL:%d,PWM:%d,PMIN:%d,PMAX:%d\r\n",
			temperatura, calefactor, pwm_ventilador, sistema_umbrales.pm_min, sistema_umbrales.pm_max);
			USART_sendString(buffer_usart);
			} else {
			USART_sendString("ERROR: Fallo de lectura en DHT11\r\n");
		}

		//Espera activa no bloqueante de 5 segundos comprobando el menú interactivo
		for (uint16_t i = 0; i < 500; i++) {
			_delay_ms(10); // 500 x 10ms = 5000ms = 5 segundos

			// Si el usuario ingresa un carácter desde el menú de la terminal serie
			if (USART_available()) {
				char opc = USART_receive();

				if (opc == 'm' || opc == 'M') {
					USART_sendString("\r\n--- MENU RECALIBRACION PUNTO MEDIO ---\r\n");
					USART_sendString("Ingrese nuevo limite inferior (ej: 16): ");
					
					// Lectura simple de 2 dígitos por USART
					char d1 = USART_receive();
					USART_transmit(d1); // Echo
					char d2 = USART_receive();
					USART_transmit(d2); // Echo
					USART_sendString("\r\n");

					int16_t nuevo_min = (d1 - '0') * 10 + (d2 - '0');

					if (actualizar_punto_medio(nuevo_min)) {
						USART_sendString(">> OK: Umbrales actualizados exitosamente.\r\n");
						} else {
						USART_sendString(">> ERROR: Limite supera la seguridad del DHT11 (50 C max).\r\n");
					}
				}
			}
		}
	}
}