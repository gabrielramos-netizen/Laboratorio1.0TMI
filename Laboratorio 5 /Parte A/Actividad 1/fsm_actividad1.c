#include "fsm_actividad1.h"

// Declaración de los prototipos de función para cada estado (Forma Punteros a Función) 
static void st_activo(void);
static void st_standby(void);
static void st_apagado(void);

// Puntero que guarda la función del estado actual 
static EstadoFuncion_t estado_actual = st_activo;

static uint32_t ultima_actividad_ms = 0;

void fsm_init(void) {
    ultima_actividad_ms = millis();
    estado_actual = st_activo;
}

void fsm_registrar_actividad(void) {
    ultima_actividad_ms = millis();
    
    // Si el sistema estaba en modo de bajo consumo, restaura los periféricos 
    if (estado_actual != st_activo) {
        power_all_enable();    // Limpia el registro PRR 
        ADCSRA |= (1 << ADEN); // Rehabilita el ADC 
        estado_actual = st_activo;
    }
}

// ESTADO ACTIVO: Tareas periódicas y control de tiempo 
static void st_activo(void) {
    // Aquí se ejecutan la lectura de sensores, actualización de LCD y procesamiento serie
    
    // Transición a STANDBY al detectar 10 segundos continuos sin actividad
    if ((millis() - ultima_actividad_ms) >= 10000UL) {
        estado_actual = st_standby;
    }
}

// ESTADO STANDBY: Bajo consumo moderado entre tareas 
static void st_standby(void) {
   // Desactivar SPI vía PRR por no ser necesario en reposo corto 
    power_spi_disable();
    
    // Entrar en SLEEP_MODE_IDLE entre ejecuciones 
    set_sleep_mode(SLEEP_MODE_IDLE);
    sleep_enable();
    sleep_cpu();
    sleep_disable();

    // Transición a APAGADO tras 30 segundos acumulados sin eventos 
    if ((millis() - ultima_actividad_ms) >= 30000UL) {
        estado_actual = st_apagado;
    }
}

// ESTADO APAGADO: Reposo profundo con apagar periféricos por PRR 
static void st_apagado(void) {
    // Desactivar ADC antes de cortar su reloj 
    ADCSRA &= ~(1 << ADEN);
    
  // Configurar el registro PRR apagar ADC, USART0, SPI, Timers y TWI 
    PRR = (1 << PRADC) | (1 << PRUSART0) | (1 << PRSPI) | 
          (1 << PRTIM1) | (1 << PRTIM2) | (1 << PRTWI);

    // Entrar en modo reposo profundo SLEEP_MODE_PWR_DOWN 
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    
    sleep_enable();
    sei();       // Habilita interrupciones globales para despertar por INT0/PCINT 
    sleep_cpu(); // La CPU duerme aquí 
    
    // AL DESPERTAR 
    sleep_disable();
    power_all_enable();
    ADCSRA |= (1 << ADEN);
    
    fsm_registrar_actividad();
}

void fsm_run(void) {
    // Ejecuta la función apuntada por el estado actual 
    estado_actual();
}
