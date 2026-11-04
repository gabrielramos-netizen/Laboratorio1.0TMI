#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include "wdt.h"
#include "timer0.h"

// Podremos ejecutar ambas partes descomentando la parte B
#define EJECUTAR_PRUEBA_A
// #define EJECUTAR_PRUEBA_B

#ifdef EJECUTAR_PRUEBA_A

volatile uint8_t evento_wdt_detectado = 0;

ISR(WDT_vect) {
    evento_wdt_detectado = 1; // Registrar el evento de despertar
}

int main(void) {
    wdt_init_despertador_8s(); // Configurar WDT como interrupción periódica
    
    while (1) {
        // 1. Entrar en Power-Down
        set_sleep_mode(SLEEP_MODE_PWR_DOWN);
        sleep_enable();
        sei();
        sleep_cpu();
        sleep_disable();
        
        //  Despertar por la interrupción del WDT
        if (evento_wdt_detectado) {
            evento_wdt_detectado = 0;
            
            // Registrar el evento y habilitar periféricos necesarios
            power_adc_enable();
            ADCSRA |= (1 << ADEN);
            
            //  Ejecutar lectura rápida de un sensor
            ADCSRA |= (1 << ADSC);
            while (ADCSRA & (1 << ADSC)); // Esperar conversión rápida
            uint16_t lectura_sensor = ADC;
            (void)lectura_sensor;
            
            // Apagar periféricos antes de volver a dormir
            ADCSRA &= ~(1 << ADEN);
            power_adc_disable();
        }
        //Volver a Power-Down
    }
}

#endif

#ifdef EJECUTAR_PRUEBA_B

int main(void) {
    timer0_init();
    
    //  Identificar la causa del reinicio mediante el registro MCUSR
    if (wdt_obtener_causa_reset() & (1 << WDRF)) {
        // El firmware recupera un estado seguro
    }
    
    // Configurar WDT en modo reset
    wdt_init_reset_2s();
    sei();
    
    uint32_t tiempo_inicio = millis();
    
    while (1) {
        wdt_reset(); // Reiniciar periódicamente el contador en funcionamiento normal
        
        // Simulación de bloqueo mediante un bucle infinito
        if ((millis() - tiempo_inicio) >= 5000UL) {
            while (1) {
                // Bucle infinito: el WDT provocará el reinicio en 2s
            }
        }
    }
}

#endif
