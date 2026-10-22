// PARTE C - LABORATORIO 4 
#ifndef F_CPU
#define F_CPU 16000000UL  // Define la frecuencia de reloj a 16 MHz (Arduino UNO)
#endif

#include <avr/io.h>       // Proporciona los nombres de los registros (DDRD, PORTD, PIND, etc.)
#include <util/delay.h>   // Proporciona funciones de retardo como _delay_ms()
#include <stdbool.h>      // Permite usar tipos de datos booleanos (true / false)
#include <stdio.h>        // Permite trabajar con cadenas de texto y formateo de datos
#include <xc.h>
#include "UART.h"
#include "UART.c"

// Utilizamos el puerto D para las salidas
#define LED_VERDE PD2 // Salida: Indica sistema armado
#define LED_ROJO PD3 // Salida: Indica Alarma Activada
#define BUZZER PD4 // Salida: Sirena de alarma 

// Entradas digitales
#define SENSOR_PIR PD5 // Entrada: Sensor de movimiento en puerto D
#define SENSOR_LDR PC0 // Entrada: Sensor de ventana en PUERTO C 


//Definimos una funcion encargada de la configuracion de pines 
void configuracion_pines(void) {
	DDRD |= (1<< LED_VERDE) | (1<< LED_ROJO) | (1<< BUZZER); // Configuramos las salidas, poniendolas en 1 
	DDRD &= ~(1<< SENSOR_PIR); // configuramos como entrada los sensores 
	DDRC &= ~(1<< SENSOR_LDR); 
	
	PORTD &= ~((1<< LED_VERDE) | (1<< LED_ROJO) | (1<< BUZZER)) // Arranca todo apagado
}



 int main(void)
{
	configuracion_pines(); // funcion anteriormente definida la llamamos dentro del main 
	uart_init(103); // llamamos a la función encargada de configurar la UART a 9600 baudios 
	
	uart_print("Estado: DESARMADO \r\n"); //Notificamos inicio por Bluetooth
	
    while(1){
		bool movimiento = (PIND & (1<< SENSOR_PIR)); // true si PD5 está en alto 
		bool luz_ventana = (PINC & (1<< SENSOR_LDR));  // true si PC0 está en alto
 		
		// MAQUINA DE ESTADOS 
		
		switch (estado_actual) {
			
			case DESARMADO:
			     PORD &= ~((1 << LED_VERDE) | (1 << LED_ROJO) | (1 << BUZZER)); //salida en bajo 
			     break;  //esperamos comando remoto desde el celular
				 
		    case ARMADO_AUTO: 
			     PORD |= (1<< LED_VERDE); // encendemos el led verde de armado, manteniendo apagado el led rojo y el buzzer 
				 PORD &= ~((1 << LED_ROJO) | (1<<BUZZER));
				 
				 if(movimiento || luz_ventana) { // evaluacion de transcisión por sensores (Detección de intrusión) 
					 PORTD |= (1<<LED_ROJO) | (1<< BUZZER); //Enciende la sirena y notifica
					 
					 if(movimiento) {
						 uart_print('ALARMA:ON;ZONA:PIR\r\n');  
					 } else {
						 uart_print('ALARMA:ON;ZONA:VENTANA\r\n'); 
					 }
						estado_actual = ALARMA_ON;  
				 }
		          break; 
		   case ARMADO_MANUAL: 
		   // Enciende LED VERDE (armado) 
		        PORTD |= (1<< LED_VERDE); 
				PORTD &= ~((1 << LED_ROJO) | (1 << BUZZER));
 	            
				break; //en modo manual no se disparan los sensores automaticos 	    
		  
		  case ALARMA_ON: 
		      // En estado de alarma suenan Sirena y LED rojo
			  PORTD |= (1<< LED_ROJO) | (1<< BUZZER); 
			  break; 
		}
		_delay_ms(100); 
	}
		
	return 0; 
	}