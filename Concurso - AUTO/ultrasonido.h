#ifndef ULTRASONIDO_H
#define ULTRASONIDO_H

#include <stdint.h>

void ultrasonido_init(void);
void ultrasonido_tarea(void);
uint8_t ultrasonido_get_cm(void);

#endif
