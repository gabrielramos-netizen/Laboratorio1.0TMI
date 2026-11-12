//====LIBRERÍAS====

#include <avr/io.h>        // Registros AVR
#include <avr/interrupt.h> // Interrupciones


//====DEFINICIÓN DE PINES Y ESTADOS====

#define PIN_LUZ PB5 // Pin 13 (LED)
#define PIN_BUZ PB4 // Pin 12 (Buzzer)
#define PIN_BOT PD2 // Pin 2  (Boton INT0)


//=====VARIABLES GLOBALES=====

volatile uint32_t t_ms  = 0; // Reloj general de 1 ms
volatile uint8_t  f_buz = 0; // Bandera de pulsador/buzzer
volatile uint16_t v_adc = 0; // Valor del potenciómetro (0-1023)
volatile uint8_t  f_adc = 0; // Bandera de lectura analógica lista


// =====INTERRUPCIONES (ISRs)=====

ISR(TIMER0_COMPA_vect) {
	t_ms++; // Incrementar reloj global
}

ISR(INT0_vect) {
	f_buz = 1; // Activa buzzer
}

ISR(TIMER1_COMPA_vect) {
	ADCSRA |= (1 << ADSC); // Disparar lectura analógica cada 100 ms
}

ISR(ADC_vect) {
	v_adc = ADC; // Lectura terminada por hardware
	f_adc = 1;
}


// =====FUNCIONES DE CONFIGURACIÓN DE HARDWARE=====

void ini_puer(void) {
	DDRB |= (1 << PIN_LUZ) | (1 << PIN_BUZ); // LUZ y BUZ como Salidas
	DDRD &= ~(1 << PIN_BOT);                 // BOT como Entrada
	PORTD |= (1 << PIN_BOT);                 // Resistencia pull-up 
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


//====PROGRAMA PRINCIPAL======

int main(void) {
	ini_puer();
	ini_tim0();
	ini_int0();
	ini_tim1();
	ini_adc();

	sei(); // Habilita interrupciones globales

	uint32_t t_luz = 0;
	uint32_t t_buz = 0;
	uint8_t  es_buz = 0;

	while (1) {

		// Tarea 1: LED a 200 ms
		if (t_ms - t_luz >= 200) {
			t_luz = t_ms;
			PORTB ^= (1 << PIN_LUZ);
		}

		// Manejo del Buzzer instantáneo por el botón
		if (f_buz == 1) {
			f_buz = 0;
			PORTB |= (1 << PIN_BUZ);
			t_buz = t_ms;
			es_buz = 1;
		}

		if (es_buz && (t_ms - t_buz >= 100)) {
			PORTB &= ~(1 << PIN_BUZ);
			es_buz = 0;
		}

		// Procesamiento de la bandera del ADC
		if (f_adc == 1) {
			f_adc = 0;
		}

	}

	return 0;
}
