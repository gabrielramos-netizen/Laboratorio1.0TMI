#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>

// Pines para el Puente H (Motor)
#define IN1 PD2
#define IN2 PD3

// Configuro el ADC
void adc_init(void) {
	ADMUX = (1 << REFS0); // Referencia AVcc (5V)
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // Prescaler 128
}

// Lectura de canal
uint16_t adc_read(uint8_t ch) {
	ADMUX = (ADMUX & 0xF0) | (ch & 0x0F);
	ADCSRA |= (1 << ADSC); //conversion
	while (ADCSRA & (1 << ADSC)); // Espera que termine
	return ADC;
}

// Configuro PWM en PB1 (Timer1)
void pwm_init(void) {
	DDRB |= (1 << PB1); // PB1 salida PWM
	
	// Fast PWM 8 bits, prescaler 64
	TCCR1A = (1 << COM1A1) | (1 << WGM10);
	TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);
	
	OCR1A = 0; // Velocidad inicial en 0
}

int main(void) {
	// Configuro salidas del motor
	DDRD |= (1 << IN1) | (1 << IN2);

	adc_init();
	pwm_init();

	uint16_t ldr = 0;
	uint16_t pot = 0;

	while (1) {
		ldr = adc_read(0); // LDR en PC0
		pot = adc_read(1); // Potenciometro en PC1

		// Giro a un sentido de prueba
		PORTD |= (1 << IN1);
		PORTD &= ~(1 << IN2);

		// Modifico velocidad (0 a 255)
		OCR1A = 150;

		_delay_ms(100);
	}
}