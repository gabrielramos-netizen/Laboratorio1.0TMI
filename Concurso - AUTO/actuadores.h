#ifndef ACTUADORES_H
#define ACTUADORES_H

#include <stdint.h>

void motores_init(void);
void manejar(int16_t v, int16_t w, uint8_t modo_lento);

void servo_init(void);
void servo_pedir(uint8_t num, uint8_t ang);
void servo_tarea(void);
void ejecutar_macro_pinza(uint8_t macro);

#endif
