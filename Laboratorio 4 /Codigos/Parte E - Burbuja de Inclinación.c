#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <math.h>

#include "twi.h"

// Configuracion de comunicacion MPU6050
#define DIRECCION_MPU_WRITE 0xD0 // Direccionvde escritura (0x68 << 1)
#define DIRECCION_MPU_READ  0xD1 // Direccion de lectura
#define REG_ACCEL_X         0x3B // Primer registro de acelerometros

float angulo_x = 0.0;
float angulo_y = 0.0;
float offset_x = 0.0;
float offset_y = 0.0;

void iniciar_mpu6050(void) {
	twi_start();
	twi_write_cmd(DIRECCION_MPU_WRITE);
	twi_write_dwr(0x6B); // Registro PWR_MGMT_1
	twi_write_dwr(0x00); // Activar sensor
	twi_stop();
}

// Lee los 6 registros del acelerometro
void leer_acelerometro(int16_t *ax, int16_t *ay, int16_t *az) {
	twi_start();
	twi_write_cmd(DIRECCION_MPU_WRITE);
	twi_write_dwr(REG_ACCEL_X);
	
	twi_repeated_start();
	twi_write_cmd(DIRECCION_MPU_READ);
	
	*ax = (int16_t)((twi_read_ack() << 8) | twi_read_ack());
	*ay = (int16_t)((twi_read_ack() << 8) | twi_read_ack());
	*az = (int16_t)((twi_read_ack() << 8) | twi_read_nack());
	
	twi_stop();
}

// Convierte aceleracion a grados de inclinacion
void calcular_angulos(void) {
	int16_t raw_x, raw_y, raw_z;
	leer_acelerometro(&raw_x, &raw_y, &raw_z);
	
	float ax_g = (float)raw_x;
	float ay_g = (float)raw_y;
	float az_g = (float)raw_z;
	
	// Formulas de inclinacion
	float ang_x_nuevo = atan2(ay_g, az_g) * (180.0 / 3.14159265);
	float ang_y_nuevo = atan2(-ax_g, sqrt(ay_g * ay_g + az_g * az_g)) * (180.0 / 3.14159265);
	
	// Restar desfase de calibracion
	ang_x_nuevo -= offset_x;
	ang_y_nuevo -= offset_y;
	
	// Filtro para suavizar
	angulo_x = (angulo_x * 0.8) + (ang_x_nuevo * 0.2);
	angulo_y = (angulo_y * 0.8) + (ang_y_nuevo * 0.2);
}

int main(void) {
	twi_init();
	iniciar_mpu6050();
	
	while (1) {
		calcular_angulos();
		_delay_ms(50);
	}
	return 0;
}