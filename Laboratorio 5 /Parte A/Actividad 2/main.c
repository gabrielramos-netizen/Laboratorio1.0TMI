#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include "wdt.h"
#include "timer0.h"

// Si quitamos el comentario de la parte B y comentamos el de la parte A podremos hacer la otra prueba 
#define EJECUTAR_PRUEBA_A
// #define EJECUTAR_PRUEBA_B

#ifdef EJECUTAR_PRUEBA_A

volatile uint8_t evento_wdt_detectado = 0;

ISR(WDT_vect) {
    evento_wdt_detectado = 1; // Registrar el evento de despertar[cite: 70]
}

int main(void) {
    wdt_init_despertador_8s(); // Configurar WDT como interrupción periódica[cite: 70]
    
    while (1) {
        // 1. Entrar en Power-Down[cite: 70]
        set_sleep_mode(SLEEP_MODE_PWR_DOWN);
        sleep_enable();
        sei();
        sleep_cpu();
        sleep_disable();
        
        // 2. Despertar por la interrupción del WDT[cite: 70]
        if (evento_wdt_detectado) {
            evento_wdt_detectado = 0;
            
            // 3. Registrar el evento y habilitar periféricos necesarios[cite: 70, 71]
            power_adc_enable();
            ADCSRA |= (1 << ADEN);
            
            // 4. Ejecutar lectura rápida de un sensor[cite: 70]
            ADCSRA |= (1 << ADSC);
            while (ADCSRA & (1 << ADSC)); // Esperar conversión rápida
            uint16_t lectura_sensor = ADC;
            (void)lectura_sensor;
            
            // Apagar periféricos antes de volver a dormir[cite: 70, 71]
            ADCSRA &= ~(1 << ADEN);
            power_adc_disable();
        }
        // 5. Volver a Power-Down[cite: 70]
    }
}

#endif

#ifdef EJECUTAR_PRUEBA_B

int main(void) {
    timer0_init();
    
    // 3. Identificar la causa del reinicio mediante el registro MCUSR[cite: 71]
    if (wdt_obtener_causa_reset() & (1 << WDRF)) {
        // 4. El firmware recupera un estado seguro[cite: 71]
    }
    
    // Configurar WDT en modo reset[cite: 71]
    wdt_init_reset_2s();
    sei();
    
    uint32_t tiempo_inicio = millis();
    
    while (1) {
        wdt_reset(); // Reiniciar periódicamente el contador en funcionamiento normal[cite: 71]
        
        // Simulación de bloqueo mediante un bucle infinito[cite: 71]
        if ((millis() - tiempo_inicio) >= 5000UL) {
            while (1) {
                // Bucle infinito: el WDT provocará el reinicio en 2s[cite: 71]
            }
        }
    }
}

#endif
