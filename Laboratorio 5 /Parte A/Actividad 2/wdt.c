#include "wdt.h"

// Variable reservada en la sección .noinit para evitar que C la sobrescriba al reiniciar
uint8_t causa_reset_mcusr __attribute__((section(".noinit")));

// Se ejecuta en la sección .init3 antes del main para capturar MCUSR y deshabilitar el WDT
void capturar_mcusr(void) __attribute__((naked)) __attribute__((section(".init3")));
void capturar_mcusr(void) {
    causa_reset_mcusr = MCUSR;
    MCUSR = 0; // Limpieza del registro MCUSR requerida por la hoja de datos
    wdt_disable(); // Desactivación preventiva del WDT[cite: 71]
}

// Prueba A: Configura el WDT en Modo Interrupción con intervalo nominal de 8 segundos
void wdt_init_despertador_8s(void) {
    cli();
    wdt_reset();
    
    // Secuencia obligatoria de cambio de configuración en WDTCSR
    WDTCSR |= (1 << WDCE) | (1 << WDE);
    
    // WDIE = 1 (Interrupción activada), WDE = 0, WDP3 = 1 y WDP0 = 1 (~8 segundos)
    WDTCSR = (1 << WDIE) | (1 << WDP3) | (1 << WDP0);
    
    sei();
}

// Prueba B: Configura el WDT en Modo Reset con intervalo nominal de 2 segundos
void wdt_init_reset_2s(void) {
    cli();
    wdt_reset();
    
    // Secuencia obligatoria de cambio de configuración
    WDTCSR |= (1 << WDCE) | (1 << WDE);
    
    // WDE = 1 (Reset por hardware), WDIE = 0, WDP2 = 1, WDP1 = 1 y WDP0 = 1 (~2 segundos)
    WDTCSR = (1 << WDE) | (1 << WDP2) | (1 << WDP1) | (1 << WDP0);
    
    sei();
}

void wdt_deshabilitar(void) {
    cli();
    wdt_reset();
    WDTCSR |= (1 << WDCE) | (1 << WDE);
    WDTCSR = 0x00;
    sei();
}

uint8_t wdt_obtener_causa_reset(void) {
    return causa_reset_mcusr;
}

// Vector de Interrupción para la Prueba A
ISR(WDT_vect) {
    // Interrupción que despierta la CPU de Power-Down sin reiniciar
}
