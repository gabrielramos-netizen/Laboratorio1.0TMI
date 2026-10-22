// PARTE C - LABORATORIO 4
#ifndef F_CPU
#define F_CPU 16000000UL  // Define la frecuencia de reloj a 16 MHz (Arduino UNO)
#endif

#include <avr/io.h>       // Proporciona los nombres de los registros (DDRD, PORTD, PIND, etc.)
#include <util/delay.h>   // Proporciona funciones de retardo como _delay_ms()
#include <stdbool.h>      // Permite usar tipos de datos booleanos (true / false)
#include <stdio.h>        // Permite trabajar con cadenas de texto y formateo de datos
#include <xc.h>
#include "UART.h"         // Librería de comunicación serie para el módulo Bluetooth

// Utilizamos el puerto D para las salidas
#define LED_VERDE PD2 // Salida: Indica sistema armado
#define LED_ROJO PD3  // Salida: Indica Alarma Activada
#define BUZZER PD4    // Salida: Sirena de alarma

// Entradas digitales
#define SENSOR_PIR PD5 // Entrada: Sensor de movimiento en puerto D
#define SENSOR_LDR PC0 // Entrada: Sensor de ventana en PUERTO C

// DEFINICIÓN DE LA MÁQUINA DE ESTADOS 
typedef enum {
	DESARMADO,          // Estado inicial: sistema desarmado
	ARMADO_AUTO,        // Estado armado automático (evalúa PIR y LDR)
	ARMADO_MANUAL,      // Estado armado manual (solo responde a mandos)
	ALARMA_ON           // Estado de intrusión detectada (sirena/LED activo)
} EstadoPlanta;

// Variable global para almacenar el estado actual del sistema
EstadoPlanta estado_actual = DESARMADO;

// Definimos una función encargada de la configuración de pines
void configuracion_pines(void) {
	DDRD |= (1<< LED_VERDE) | (1<< LED_ROJO) | (1<< BUZZER); // Configuramos las salidas, poniéndolas en 1
	DDRD &= ~(1<< SENSOR_PIR); // Configuramos como entrada los sensores
	DDRC &= ~(1<< SENSOR_LDR);
	
	PORTD &= ~((1<< LED_VERDE) | (1<< LED_ROJO) | (1<< BUZZER)); // Arranca todo apagado
}

int main(void)
{
	configuracion_pines(); // Función anteriormente definida la llamamos dentro del main
	uart_init(103);        // Llamamos a la función encargada de configurar la UART a 9600 baudios
	
	uart_print("ESTADO:DESARMADO\r\n"); // Notificamos inicio por Bluetooth
	
	while(1){
		bool movimiento = (PIND & (1<< SENSOR_PIR)); // true si PD5 está en alto
		bool luz_ventana = (PINC & (1<< SENSOR_LDR));  // true si PC0 está en alto
		
		// RECEPCIÓN NO BLOQUEANTE DE COMANDOS VÍA BLUETOOTH
		
		// Verificamos si el registro de recepción UART tiene un dato disponible
		if (UCSR0A & (1 << RXC0)) {
			char comando = uart_receive(); // Leemos el caracter enviado por la aplicación
			
			// Evaluamos las transiciones accionadas por comandos remotos
			switch (comando) {
				case 'A': // Comando para Armado Automático ("ARM_AUTO")
				estado_actual = ARMADO_AUTO;
				uart_print("ESTADO:ARMADO_AUTO\r\n");
				break;
				
				case 'M': // Comando para Armado Manual ("ARM_MANUAL")
				estado_actual = ARMADO_MANUAL;
				uart_print("ESTADO:ARMADO_MANUAL\r\n");
				break;
				
				case 'D': // Comando para Desarmar ("DESARMAR")
				estado_actual = DESARMADO;
				uart_print("ESTADO:DESARMADO\r\n");
				break;
				
				case 'S': // Comando para Silenciar ("SILENCIO")
				// Silenciamos la sirena apaliando las salidas
				PORTD &= ~((1 << LED_ROJO) | (1 << BUZZER));
				if (estado_actual == ALARMA_ON) {
					estado_actual = ARMADO_AUTO; // Regresa al modo armado activo[cite: 1]
				}
				uart_print("ESTADO:SILENCIADO\r\n");
				break;
				
				case 'F': // Comando para Forzar Alarma ("FORZAR_ALARMA")
				if (estado_actual == ARMADO_MANUAL) {
					PORTD |= (1 << LED_ROJO) | (1 << BUZZER);
					estado_actual = ALARMA_ON;
					uart_print("ALARMA:ON;ZONA:FORZADO\r\n");
				}
				break;
				
				case 'R': // Comando para Reiniciar ("REINICIAR")
				PORTD &= ~((1 << LED_VERDE) | (1 << LED_ROJO) | (1 << BUZZER));
				estado_actual = DESARMADO;
				uart_print("ESTADO:DESARMADO\r\n");
				break;
			}
		}
		
		// MAQUINA DE ESTADOS FINITOS (Mealy)
		switch (estado_actual) {
			
			case DESARMADO:
			PORTD &= ~((1 << LED_VERDE) | (1 << LED_ROJO) | (1 << BUZZER)); // Salidas en bajo
			break; // Esperamos comando remoto desde el celular
			
			case ARMADO_AUTO:
			PORTD |= (1<< LED_VERDE); // Encendemos el LED verde de armado, manteniendo apagado el LED rojo y el buzzer
			PORTD &= ~((1 << LED_ROJO) | (1<<BUZZER));
			
			if(movimiento || luz_ventana) { // Evaluación de transición por sensores (Detección de intrusión)
				PORTD |= (1<<LED_ROJO) | (1<< BUZZER); // Enciende la sirena y notifica
				
				if(movimiento) {
					uart_print("ALARMA:ON;ZONA:PIR\r\n");
					} else {
					uart_print("ALARMA:ON;ZONA:VENTANA\r\n");
				}
				estado_actual = ALARMA_ON;
			}
			break;
			
			case ARMADO_MANUAL:
			// Enciende LED VERDE (armado)
			PORTD |= (1<< LED_VERDE);
			PORTD &= ~((1 << LED_ROJO) | (1 << BUZZER));
			break; // En modo manual no se disparan los sensores automáticos
			
			case ALARMA_ON:
			// En estado de alarma suenan Sirena y LED rojo
			PORTD |= (1<< LED_ROJO) | (1<< BUZZER);
			break;
		}
		
		_delay_ms(100); // Retardo para estabilizar el bucle
	}
	
	return 0;
}