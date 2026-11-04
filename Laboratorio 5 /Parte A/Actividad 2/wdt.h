#ifndef WDT_H_
#define WDT_H_

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>

// Variable global que almacena la causa del reset obtenida en la inicialización 
extern uint8_t causa_reset_mcusr;

// Inicialización de la Prueba A: WDT como despertador periódico (Interrupción ~8s) 
void wdt_init_despertador_8s(void);

// Inicialización de la Prueba B: WDT como mecanismo de recuperación (Reset ~2s)
void wdt_init_reset_2s(void);

// Desactiva el Watchdog Timer 
void wdt_deshabilitar(void);

// Devuelve la lectura del registro MCUSR guardada al arrancar
uint8_t wdt_obtener_causa_reset(void);

#endif // WDT_H_ 
