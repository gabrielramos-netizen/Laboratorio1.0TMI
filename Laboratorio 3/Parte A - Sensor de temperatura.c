// PARTE A - LAB 3 | TMI 
#include <avr/io.h>
#include <stdio.h>
#include <xc.h>
#include <stdbool.h>


#define F_CPU 16000000UL //frecuencia del UC
#define DHT11_MAX_TEMP 50 // temperatura maxima que percibe el sensor DHT11 

typedef struct {
	int16_t pm_min; // Inicio de punto medio 
	int16_t pm_max; // Fin de punto medio 
} UmbralesTemperatura; // usando typedef definimos el nuevo tipo de datos 

UmbralesTemperatura sistema_umbrales = {16, 25}; // pm_min = 16, pm_max = 25 --> para el punto medio 

// Función para intentar actualizar el punto medio
bool actualizar_punto_medio(int16_t nuevo_min) {
	int16_t nuevo_max = nuevo_min + 9; // Ancho de 10°C (ej. 16 a 25)
	
	// Verificamos que la velocidad alta no supere el límite del DHT11 (50°C)
	// Alta velocidad arranca a partir de (nuevo_max + 21)
	if ((nuevo_max + 21) > DHT11_MAX_TEMP) {
		return false; // Cambio no permitido por seguridad del sensor
	}
	
	sistema_umbrales.pm_min = nuevo_min;
	sistema_umbrales.pm_max = nuevo_max;
	return true; // Se esta dentro del rango aceptado y se actualizan los umbrales de temperatura 
}

//definimos una funcion para la comunicacion serial 

void USART_init(unsigned int baud) {
	unsigned int ubrr = F_CPU/16/baud - 1; // ubrr = FCU/16*BAUDRATE - 1


//configuramos la tasa de baudios usando la parte baja y alta del registro UBRR0

UBRR0H = (unsigned char)(ubrr >> 8); 
UBRR0L = (unsigned char)ubrr;  
 

// habilitamos la transmision y recepcion en los pine TX Y RX
UCSR0B = (1 << RXEN0) | (1 << TXEN0); 

// Se tiene 8 bits de datos, 1 bit de parada, y sin paridad 
UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); 
}

void USART_transmit(unsigned char data){
	//esperamos al que el buffer de transmision esté vacío 
	
	while(!(UCSR0A & (1 << UDRE0))); // UCSROA = 0b000010000, UDRE0 es el bit 5, se puso en 1, entonces while(0) y entonces se puede transmitir un nuevo dato 
	// colocamos el dato en el registro para enviarlo 
	UDR0 = data; 
	
}

unsigned char USART_receive(void) {
	//Espera a que los datos sean recibidos 
     while(!(UCSR0A &( 1<< RXC0))); // parecido al while anterior
	 //Retorna el dato leido 
	 return UDR0; 	
}

void USART_sendString(const char*str){
	while(*str){ // cuando encuentre \0 se detiene el bucle, es decir mostro todo el mensaje, o sea while(0)
		USART_transmit(*str++); // str++: Incrementa el puntero en 1 para apuntar a la siguiente letra de la cadena.
	}
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



int main(void)
{
    while(1)
    {
    }
}
