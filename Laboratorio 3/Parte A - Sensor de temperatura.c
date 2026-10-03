// PARTE A LAB 3
#include <avr/io.h>
#include <stdio.h>
#include <xc.h>

#define F_CPU 16000000UL //frecuencia del UC
#define DHT11_MAX_TEMP 50 // temperatura maxima que percibe el sensor DHT11 


//definimos una funcion para la comunicacion serial 

void USART_init(unsigned int baud) {
	unsigned int ubrr = F_CPU/16/baud - 1; // ubrr = FCU/16*BAUDRATE - 1


//configuramos la tasa de baudios usando la parte baja y alta del registro UBRR0

UBRR0H = (unsigned char)(ubrr >> 8); 
UBRR0L = (unsigned char)ubrr;  
 

// habilitamos la transmision y recepcion en los pine TX Y RX
UCSR0B = (1<<RXEN0) | (1<< TXEN0); 

// Se tiene 8 bits de datos, 1 bit de parada, y sin paridad 
UCSR0C = (1<<UCSZ01) | (1<<UCSZ00); 
}

void USART_transmit(unsigned char data){
	//esperamos al que el buffer de transmision esté vacío 
	
	while(!(UCSR0A & (1<<UDRE0))); 
	// colocamos el dato en el registro para enviarlo 
	UDR0 = data; 
	
}

unsigned char USART_receive(void) {
	//Espera a que los datos sean recibidos 
     while(!(UCSR0A &(1<<RXC0))); 
	 //Retorna el dato leido 
	 return UDR0; 	
}

void USART_sendString(const char*str){
	while(*str){
		USART_transmit(*str++); 
	}
}






int main(void)
{
    while(1)
    {
    }
}