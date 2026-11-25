#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include "actuadores.h"

// Pines Puente H L298N
#define IN1_PIN     PD2
#define IN2_PIN     PD3
#define IN3_PIN     PD4
#define IN4_PIN     PD7
#define ENA_PIN     PD6   // OC0A (PWM Motor Izquierdo)
#define ENB_PIN     PD5   // OC0B (PWM Motor Derecho)

// Pines 3 Servomotores (Multiplexados en Timer 1 a 50 Hz)
#define SERVO1_PIN  PB1   // Pinza (Abrir / Cerrar)
#define SERVO2_PIN  PB2   // Elevación (Subir / Bajar)
#define SERVO3_PIN  PB3   // Carro lineal (Adelante / Atrás)

// Parámetros de calibración mecánica (ajustar según pruebas en pista)
#define ZONA_MUERTA         10      // Ignora consignas menores al 10%
#define PWM_MIN_ARRANQUE    60      // PWM mínimo real para vencer fricción estática (0..255)
#define PASO_SERVO          2       // Rampa suave: 2 grados cada 20 ms (criterio de cátedra)

//  MOTORES DC (Timer 0 Fast PWM) 
void motores_init(void) {
    DDRD |= (1 << IN1_PIN) | (1 << IN2_PIN) | (1 << IN3_PIN) | (1 << IN4_PIN) |
            (1 << ENA_PIN) | (1 << ENB_PIN);
    TCCR0A = (1 << COM0A1) | (1 << COM0B1) | (1 << WGM01) | (1 << WGM00);
    TCCR0B = (1 << CS01) | (1 << CS00); // Prescaler 64 -> ~976.6 Hz
    OCR0A = 0;
    OCR0B = 0;
}

static void set_rueda_izq(int16_t pwm) {
    if (pwm > 0) {
        PORTD |= (1 << IN1_PIN);
        PORTD &= ~(1 << IN2_PIN);
        OCR0A = (uint8_t)pwm;
    } else if (pwm < 0) {
        PORTD |= (1 << IN2_PIN);
        PORTD &= ~(1 << IN1_PIN);
        OCR0A = (uint8_t)(-pwm);
    } else {
        PORTD &= ~((1 << IN1_PIN) | (1 << IN2_PIN));
        OCR0A = 0;
    }
}

static void set_rueda_der(int16_t pwm) {
    if (pwm > 0) {
        PORTD |= (1 << IN3_PIN);
        PORTD &= ~(1 << IN4_PIN);
        OCR0B = (uint8_t)pwm;
    } else if (pwm < 0) {
        PORTD |= (1 << IN4_PIN);
        PORTD &= ~(1 << IN3_PIN);
        OCR0B = (uint8_t)(-pwm);
    } else {
        PORTD &= ~((1 << IN3_PIN) | (1 << IN4_PIN));
        OCR0B = 0;
    }
}

// Mapea linealmente cualquier comando activo [1..255] al rango útil [PWM_MIN_ARRANQUE..255]
// para que el modo lento conserve resolución y no quede pegado en el mínimo
static int32_t escalar_zona_muerta(int32_t pwm) {
    if (pwm == 0) return 0;
    int32_t signo = (pwm > 0) ? 1 : -1;
    int32_t mag = (pwm > 0) ? pwm : -pwm;

    if (mag > 255) mag = 255;
    mag = PWM_MIN_ARRANQUE + ((mag * (255 - PWM_MIN_ARRANQUE)) / 255);
    return signo * mag;
}

void manejar(int16_t v, int16_t w, uint8_t modo_lento) {
    if (v > -ZONA_MUERTA && v < ZONA_MUERTA) v = 0;
    if (w > -ZONA_MUERTA && w < ZONA_MUERTA) w = 0;

    if (modo_lento) {
        v /= 2;
        w /= 2;
    }

    int32_t izq = ((int32_t)(v + w) * 255) / 100;
    int32_t der = ((int32_t)(v - w) * 255) / 100;

    izq = escalar_zona_muerta(izq);
    der = escalar_zona_muerta(der);

    set_rueda_izq((int16_t)izq);
    set_rueda_der((int16_t)der);
}

// --- 3 SERVOMOTORES (Timer 1 CTC Multiplexado a 50 Hz) ---
static volatile uint16_t servo_ticks[3] = {2999, 2999, 2999};
static uint8_t servo_pos[3] = {130, 140, 10};
static uint8_t servo_obj[3] = {130, 140, 10};

void servo_init(void) {
    DDRB |= (1 << SERVO1_PIN) | (1 << SERVO2_PIN) | (1 << SERVO3_PIN);
    TCCR1A = 0;
    TCCR1B = (1 << WGM12) | (1 << CS11); // CTC, Prescaler 8 (0.5 us/tick)
    OCR1A  = 3999;
    TIMSK1 = (1 << OCIE1A);
}

ISR(TIMER1_COMPA_vect) {
    static uint8_t idx = 0;
    static uint32_t acum = 0;

    PORTB &= ~((1 << SERVO1_PIN) | (1 << SERVO2_PIN) | (1 << SERVO3_PIN));

    if (idx < 3) {
        if (idx == 0) PORTB |= (1 << SERVO1_PIN);
        else if (idx == 1) PORTB |= (1 << SERVO2_PIN);
        else PORTB |= (1 << SERVO3_PIN);

        OCR1A = servo_ticks[idx];
        acum += servo_ticks[idx];
        idx++;
    } else {
        OCR1A = (acum < 40000) ? (uint16_t)(40000 - acum) : 1000;
        acum = 0;
        idx = 0;
    }
}

void servo_pedir(uint8_t num, uint8_t ang) {
    if (num < 3) {
        if (ang > 180) ang = 180;
        servo_obj[num] = ang;
    }
}

void servo_tarea(void) {
    for (uint8_t i = 0; i < 3; i++) {
        if (servo_pos[i] + PASO_SERVO <= servo_obj[i]) servo_pos[i] += PASO_SERVO;
        else if (servo_pos[i] >= servo_obj[i] + PASO_SERVO) servo_pos[i] -= PASO_SERVO;
        else servo_pos[i] = servo_obj[i];

        // Rango de pulso: 0.5 ms (1000 ticks) a 2.5 ms (5000 ticks) para 0..180 grados
        uint16_t ticks = 1000 + ((uint32_t)servo_pos[i] * 4000) / 180;
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
            servo_ticks[i] = ticks;
        }
    }
}

void ejecutar_macro_pinza(uint8_t macro) {
    if (macro == 0) {        // GUARDADO (dentro de caja 17x23 cm)
        servo_pedir(0, 130); servo_pedir(1, 140); servo_pedir(2, 10);
    } else if (macro == 1) { // PREPARAR RECOGIDA (pinza abierta y carro extendido)
        servo_pedir(0, 40);  servo_pedir(2, 150); servo_pedir(1, 70);
    } else if (macro == 2) { // CAPTURAR Y SUBIR (cierra pinza, eleva y retrae)
        servo_pedir(0, 125); servo_pedir(1, 135); servo_pedir(2, 40);
    } else if (macro == 3) { // ENTREGAR EN B (extiende, baja y abre pinza)
        servo_pedir(2, 150); servo_pedir(1, 75);  servo_pedir(0, 40);
    }
}
