//====LIBRERÍAS====

#define F_CPU 16000000UL   // Frecuencia del reloj (16 MHz) necesaria para util/delay.h
#include <avr/io.h>        // Registros AVR
#include <avr/interrupt.h> // Interrupciones
#include <util/delay.h>    // Retardos de microsegundos para protocolos I2C y DHT11


//====DEFINICIÓN DE PINES Y ESTADOS====

#define PIN_LUZ PB5 // Pin 13 (LED)
#define PIN_BUZ PB4 // Pin 12 (Buzzer)
#define PIN_BOT PD2 // Pin 2  (Boton INT0)
#define PIN_DHT PD4 // Pin 4  (Sensor DHT11)

// Dirección I2C del módulo PCF8574T (cambiar a 0x3F si usa PCF8574AT)
#define LCD_DIR 0x27

// Codificación numérica de la FSM
#define EST_ACT 0   // Modo ACTIVO
#define EST_STB 1   // Modo ESPERA
#define EST_APA 2   // Modo APAGADO


//=====VARIABLES GLOBALES=====

volatile uint32_t t_ms    = 0;    // Reloj general de 1 ms
volatile uint8_t  f_buz   = 0;    // Bandera para activar buzzer
volatile uint8_t  f_bot   = 0;    // Bandera de evento de pulsador
volatile uint16_t v_adc   = 0;    // Valor del potenciómetro (0-1023)
volatile uint8_t  f_adc   = 0;    // Bandera de lectura analógica lista
uint8_t           v_tem   = 0;    // Temperatura leída en °C
uint8_t           lcd_luz = 0x08; // Bit P3 del PCF8574T (0x08 = Luz ON, 0x00 = Luz OFF)

uint8_t  est_act = EST_ACT;       // Estado actual de la FSM
uint32_t t_act   = 0;             // Marca de tiempo de última actividad del usuario


// =====INTERRUPCIONES (ISRs)=====

ISR(TIMER0_COMPA_vect) {
	t_ms++; // Incrementar reloj global
}

ISR(INT0_vect) {
	f_buz = 1; // Señaliza activación de buzzer
	f_bot = 1; // Señaliza evento de usuario sin alterar variables de 32 bits en la ISR
}

ISR(TIMER1_COMPA_vect) {
	ADCSRA |= (1 << ADSC); // Disparar lectura analógica cada 100 ms
}

ISR(ADC_vect) {
	v_adc = ADC; // Lectura terminada por hardware
	f_adc = 1;
}


// =====FUNCIONES DE CONFIGURACIÓN Y UTILIDAD=====

uint32_t get_ms(void) {
	uint32_t val;
	cli();
	val = t_ms;
	sei();
	return val;
}

void ini_puer(void) {
	DDRB |= (1 << PIN_LUZ) | (1 << PIN_BUZ); // LUZ y BUZ como Salidas
	DDRD &= ~(1 << PIN_BOT);                 // BOT como Entrada
	PORTD |= (1 << PIN_BOT);                 // Resistencia pull-up interna
}

void ini_tim0(void) {
	TCCR0A = (1 << WGM01);              // Modo CTC
	TCCR0B = (1 << CS01) | (1 << CS00); // Prescaler 64
	OCR0A  = 249;                       // Alarma a 1 ms
	TIMSK0 = (1 << OCIE0A);             // Habilitar alarma
}

void ini_int0(void) {
	EICRA |= (1 << ISC01);  // Flanco descendente
	EICRA &= ~(1 << ISC00);
	EIMSK |= (1 << INT0);   // Habilitar INT0
}

void ini_tim1(void) {
	TCCR1A = 0;
	TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10); // Prescaler 64, Modo CTC
	OCR1A  = 24999;                                   // 100 ms
	TIMSK1 = (1 << OCIE1A);                           // Habilitar alarma
}

void ini_adc(void) {
	ADMUX  = (1 << REFS0); // 5V referencia canal A0
	ADCSRA = (1 << ADEN) | (1 << ADIE) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}


// =====CONTROLADOR DE PANTALLA LCD POR I2C (PCF8574T)=====

void ini_i2c(void) {
	TWSR = 0x00; // Prescaler = 1
	TWBR = 72;   // Frecuencia SCL = 100 kHz a 16 MHz
	TWCR = (1 << TWEN);
}

void env_i2c(uint8_t dat) {
	uint16_t lim = 0;

	TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
	while (!(TWCR & (1 << TWINT))) { if (++lim > 1000) return; }

	TWDR = (LCD_DIR << 1); // Dirección del esclavo + escritura (0)
	TWCR = (1 << TWINT) | (1 << TWEN);
	lim = 0;
	while (!(TWCR & (1 << TWINT))) { if (++lim > 1000) return; }

	TWDR = dat;
	TWCR = (1 << TWINT) | (1 << TWEN);
	lim = 0;
	while (!(TWCR & (1 << TWINT))) { if (++lim > 1000) return; }

	TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}

// Envía 4 bits al PCF8574T (P4-P7 = datos, P2 = En, P0 = RS, P3 = Luz)
void env_nib_i2c(uint8_t nib, uint8_t rs) {
	uint8_t base = (nib << 4) | rs | lcd_luz;
	env_i2c(base | 0x04);  // Pulso En = 1
	_delay_us(5);
	env_i2c(base & ~0x04); // Pulso En = 0
	_delay_us(50);
}

void cmd_lcd(uint8_t cmd) {
	env_nib_i2c(cmd >> 4, 0x00);
	env_nib_i2c(cmd & 0x0F, 0x00);
	if (cmd == 0x01 || cmd == 0x02) {
		_delay_ms(2);
	}
}

void dat_lcd(uint8_t dat) {
	env_nib_i2c(dat >> 4, 0x01);
	env_nib_i2c(dat & 0x0F, 0x01);
}

void ini_lcd(void) {
	ini_i2c();
	_delay_ms(50);

	env_nib_i2c(0x03, 0x00); _delay_ms(5);
	env_nib_i2c(0x03, 0x00); _delay_ms(5);
	env_nib_i2c(0x03, 0x00); _delay_ms(5);
	env_nib_i2c(0x02, 0x00); _delay_ms(5); // Activar modo 4 bits

	cmd_lcd(0x28); // 2 líneas, matriz 5x8
	cmd_lcd(0x0C); // Pantalla encendida, cursor apagado
	cmd_lcd(0x06); // Avance automático del cursor
	cmd_lcd(0x01); // Limpiar pantalla
	_delay_ms(5);
}

void txt_lcd(const char* txt) {
	while (*txt) {
		dat_lcd(*txt++);
	}
}

void num_lcd(uint16_t num, uint8_t dig) {
	if (dig == 4) {
		dat_lcd(((num / 1000) % 10) + '0');
		dat_lcd(((num / 100) % 10) + '0');
	}
	dat_lcd(((num / 10) % 10) + '0');
	dat_lcd((num % 10) + '0');
}

void ver_lcd(const char* lin1, uint8_t ver_dat) {
	cmd_lcd(0x80); // Fila 1, Columna 0
	txt_lcd(lin1);

	cmd_lcd(0xC0); // Fila 2, Columna 0
	if (ver_dat == 1) {
		txt_lcd("ADC:");
		num_lcd(v_adc, 4);
		txt_lcd(" T:");
		num_lcd(v_tem, 2);
		txt_lcd("C  ");
	} else {
		txt_lcd("                ");
	}
}


// =====CONTROLADOR DEL SENSOR DHT11=====

void lee_dht(void) {
	uint8_t dat[5] = {0, 0, 0, 0, 0};
	uint16_t lim = 0;

	DDRD |= (1 << PIN_DHT);   // PD4 como Salida
	PORTD &= ~(1 << PIN_DHT); // Mandar 0V
	_delay_ms(18);
	PORTD |= (1 << PIN_DHT);  // Mandar 5V
	_delay_us(30);
	DDRD &= ~(1 << PIN_DHT);  // PD4 como Entrada

	cli(); // Pausar interrupciones solo durante la captura de 40 bits

	lim = 0;
	while (PIND & (1 << PIN_DHT)) { if (++lim > 1000) { sei(); return; } }
	lim = 0;
	while (!(PIND & (1 << PIN_DHT))) { if (++lim > 1000) { sei(); return; } }
	lim = 0;
	while (PIND & (1 << PIN_DHT)) { if (++lim > 1000) { sei(); return; } }

	for (uint8_t i = 0; i < 40; i++) {
		lim = 0;
		while (!(PIND & (1 << PIN_DHT))) { if (++lim > 1000) { sei(); return; } }
		_delay_us(35);
		if (PIND & (1 << PIN_DHT)) {
			dat[i / 8] |= (1 << (7 - (i % 8)));
		}
		lim = 0;
		while (PIND & (1 << PIN_DHT)) { if (++lim > 1000) { sei(); return; } }
	}

	sei(); // Reactivar interrupciones

	if ((uint8_t)(dat[0] + dat[1] + dat[2] + dat[3]) == dat[4]) {
		v_tem = dat[2];
	}
}


//=====FUNCIONES DE CADA ESTADO DE LA FSM=====

static uint32_t t_luz = 0;
static uint32_t t_lcd = 0;
static uint32_t t_tem = 0;

// Estado 0: ACTIVO (Ejecuta las 5 tareas periódicas)
void run_act(void) {
	uint32_t t_ahora = get_ms();
	lcd_luz = 0x08; // Retroiluminación encendida

	if (t_ahora - t_luz >= 200) {
		t_luz = t_ahora;
		PORTB ^= (1 << PIN_LUZ);
	}

	if (t_ahora - t_lcd >= 500) {
		t_lcd = t_ahora;
		ver_lcd("MODO: ACTIVO    ", 1);
	}

	if (t_ahora - t_tem >= 2000) {
		t_tem = t_ahora;
		lee_dht();
	}
}

// Estado 1: ESPERA (Reduce actividad y muestra «Esperando...»)
void run_stb(void) {
	uint32_t t_ahora = get_ms();
	lcd_luz = 0x08;           // Mantiene retroiluminación encendida
	PORTB &= ~(1 << PIN_LUZ); // LED apagado

	if (t_ahora - t_lcd >= 500) {
		t_lcd = t_ahora;
		ver_lcd("Esperando...    ", 0);
	}
}

// Estado 2: APAGADO (Detiene periféricos y mantiene luz LCD para lectura)
void run_apa(void) {
	uint32_t t_ahora = get_ms();
	lcd_luz = 0x08;           // Mantiene luz de fondo encendida 
	PORTB &= ~(1 << PIN_LUZ); // LED apagado

	if (t_ahora - t_lcd >= 500) {
		t_lcd = t_ahora;
		ver_lcd("SISTEMA APAGADO ", 0);
	}
}

// Tabla de punteros a función (Sin switch-case)
typedef void (*func_est)(void);
func_est fsm[3] = {run_act, run_stb, run_apa};

// Evaluación de las condiciones temporales de transición
void eval_fsm(uint32_t t_ahora) {
	uint32_t inactividad = t_ahora - t_act;

	// Transición de ACTIVO a ESPERA: 10 segundos sin actividad del usuario
	if (est_act == EST_ACT && inactividad >= 10000) {
		est_act = EST_STB;
	}
	// Transición a APAGADO: 30 segundos contados desde el último evento del usuario
	else if (est_act == EST_STB && inactividad >= 30000) {
		est_act = EST_APA;
	}
}


//====PROGRAMA PRINCIPAL======

int main(void) {
	ini_puer();
	ini_tim0();
	ini_int0();
	ini_tim1();
	ini_adc();
	ini_lcd();

	sei(); // Habilita interrupciones globales

	uint32_t t_buz  = 0;
	uint8_t  es_buz = 0;

	while (1) {
		uint32_t t_ahora = get_ms();

		// Procesamiento sincronizado de la pulsación (evita condición de carrera)
		if (f_bot == 1) {
			f_bot   = 0;
			t_act   = t_ahora;
			est_act = EST_ACT;
		}

		// Manejo del Buzzer instantáneo por el botón
		if (f_buz == 1) {
			f_buz = 0;
			PORTB |= (1 << PIN_BUZ);
			t_buz  = t_ahora;
			es_buz = 1;
		}

		if (es_buz && (t_ahora - t_buz >= 100)) {
			PORTB &= ~(1 << PIN_BUZ);
			es_buz = 0;
		}

		// Procesamiento de la bandera del ADC
		if (f_adc == 1) {
			f_adc = 0;
		}

		// Evaluador de Inactividad y Transiciones de Estado
		eval_fsm(t_ahora);

		// Ejecución de la FSM
		fsm[est_act]();
	}

	return 0;
}
