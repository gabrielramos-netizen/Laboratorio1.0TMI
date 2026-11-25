#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "tick.h"
#include "uart.h"
#include "actuadores.h"
#include "ultrasonido.h"

// Definimos el pin del LED integrado (PB5 / D13) para dar feedback visual del estado
// del enlace Bluetooth sin depender únicamente de la pantalla del celular
#define LED_PIN             PB5

// Definimos los bytes de cabecera de nuestras tramas de 5 bytes:
// Usamos 0xAA para comandos entrantes y 0xBB para telemetría saliente, de modo que
// nunca se confunda un mensaje de control con uno de diagnóstico
#define TRAMA_INICIO_RX     0xAA
#define TRAMA_INICIO_TX     0xBB

// Establecemos el tiempo máximo de tolerancia sin recibir tramas en 300 ms.
// Elegimos este valor porque supera ampliamente la latencia máxima del enlace Bluetooth
// medida en pruebas, pero detiene el robot a tiempo antes de que salga de pista
#define FAILSAFE_MS         300

// Fijamos en 6 cm el umbral de detección frontal del sensor ultrasónico HC-SR04P
// para evitar colisiones con las paredes del circuito o con los estantes de carga
#define DIST_FRENO_CM       6

// Variables globales de estado del módulo main (declaradas static para encapsularlas
// dentro de este archivo y evitar colisiones de nombres con nuestras librerías)
static int16_t v_cmd = 0;                // Consigna de velocidad lineal recibida (-100 a 100)
static int16_t w_cmd = 0;                // Consigna de velocidad angular recibida (-100 a 100)
static uint8_t modo_lento = 0;           // Bandera de precisión: 1 reduce la velocidad a la mitad
static uint8_t anticolision_on = 1;      // Habilita por defecto la asistencia de frenado frontal
static uint8_t hubo_trama = 0;           // Indica si ya recibimos al menos una trama válida
static uint32_t t_ultima_trama = 0;      // Marca de tiempo (en ms) de la última trama válida
static uint8_t en_failsafe = 0;          // Indica si actualmente estamos en corte de enlace
static uint8_t hubo_fs_flag = 0;         // Registra si ocurrió un failsafe para avisar a la App
static uint8_t id_estado_telemetria = 0; // Código numérico del estado actual enviado en telemetría

// Parser de tramas serie mediante Tabla Estado x Evento en memoria Flash (PROGMEM)
// Decidimos implementar esta máquina de estados como una matriz bidimensional constante
// en Flash en lugar de un switch-case para ahorrar memoria SRAM del ATmega328P y
// garantizar tiempos de transición constantes y determinísticos
enum { RX_INICIO, RX_CMD, RX_D1, RX_D2, RX_CHK, N_ESTADOS_RX };
enum { EV_BYTE_INICIO, EV_BYTE_DATO, N_EVENTOS_RX };

static const uint8_t tabla_rx_sig[N_ESTADOS_RX][N_EVENTOS_RX] PROGMEM = {
    // Desde RX_INICIO: solo avanzamos si llega la cabecera 0xAA; cualquier otro byte se ignora
    { RX_CMD,    RX_INICIO },
    // Desde RX_CMD: guardamos el código de comando y pasamos a esperar el primer dato
    { RX_D1,     RX_D1     },
    // Desde RX_D1: guardamos el primer byte de datos y pasamos al segundo byte
    { RX_D2,     RX_D2     },
    // Desde RX_D2: guardamos el segundo byte de datos y pasamos a esperar el checksum
    { RX_CHK,    RX_CHK    },
    // Desde RX_CHK: tras verificar la integridad de la trama volvemos siempre a esperar cabecera
    { RX_INICIO, RX_INICIO }
};

static uint8_t rx_estado = RX_INICIO;
static uint8_t rx_cmd, rx_d1, rx_d2;

// Esta función interpreta y aplica los parámetros de una trama ya validada por checksum.
// Solo actualiza variables de consigna o dispara acciones de pinza; no mueve los motores
// directamente para que toda decisión de tracción pase por la máquina de estados principal
static void ejecutar_comando(uint8_t cmd, uint8_t d1, uint8_t d2) {
    if (cmd == 0x01 && d1 <= 200 && d2 <= 200) {
        // Restamos 100 porque los bytes viajan sin signo (0 a 200) desde App Inventor;
        // de este modo recuperamos el rango con signo (-100 a +100) con 100 como reposo
        v_cmd = (int16_t)d1 - 100;
        w_cmd = (int16_t)d2 - 100;
    } else if (cmd == 0x02) {
        // Ejecutamos una de las 4 secuencias predefinidas de la pinza (Guardado, Preparar,
        // Capturar o Entregar) utilizando rampas suaves no bloqueantes
        ejecutar_macro_pinza(d1);
    } else if (cmd == 0x03) {
        // Actualizamos el modo de velocidad (rápido/lento) y el permiso del freno ultrasónico
        modo_lento = (d1 != 0) ? 1 : 0;
        anticolision_on = (d2 != 0) ? 1 : 0;
    } else if (cmd == 0x04) {
        // Permitimos el ajuste fino individual de cualquiera de los 3 servos (0, 1 o 2)
        // enviando directamente el ángulo deseado entre 0 y 180 grados
        servo_pedir(d1, d2);
    }
}

// Vaciamos el buffer circular de recepción UART byte a byte sin bloquear el CPU.
// Por cada byte leemos su valor, clasificamos si es cabecera o dato y consultamos el
// siguiente estado directamente en la tabla almacenada en Flash con pgm_read_byte
void parser_tarea(void) {
    while (uart_hay()) {
        uint8_t c = uart_leer();
        uint8_t estado_previo = rx_estado;

        if (estado_previo == RX_CMD) {
            rx_cmd = c;
        } else if (estado_previo == RX_D1) {
            rx_d1 = c;
        } else if (estado_previo == RX_D2) {
            rx_d2 = c;
        } else if (estado_previo == RX_CHK) {
            // Verificamos el checksum XOR entre comando y datos. Si coincide, la trama
            // es íntegra: ejecutamos la orden y alimentamos el reloj del failsafe
            if (c == (rx_cmd ^ rx_d1 ^ rx_d2)) {
                ejecutar_comando(rx_cmd, rx_d1, rx_d2);
                t_ultima_trama = millis();
                hubo_trama = 1;
            }
        }

        uint8_t ev = (c == TRAMA_INICIO_RX) ? EV_BYTE_INICIO : EV_BYTE_DATO;
        rx_estado = pgm_read_byte(&tabla_rx_sig[estado_previo][ev]);
    }
}

// Maquina de estados 

// Máquina de Estados Principal mediante Punteros a Función
// Implementamos la FSM del robot con un puntero a función
// para encapsular el comportamiento de cada estado en su propia función independiente,
// facilitando el mantenimiento y permitiendo probar cada estado por separado
static void (*estado_actual)(void);

static void st_inicio(void);
static void st_espera_enlace(void);
static void st_manual(void);
static void st_freno_prox(void);
static void st_failsafe(void);

// Estado inicial del sistema: nos aseguramos de que los motores arranquen detenidos
// y llevamos los 3 servos a la posición de guardado para cumplir con la caja de
// inspección técnica de 17 x 23 cm antes de pasar a esperar conexión
static void st_inicio(void) {
id_estado_telemetria = 0;
manejar(0, 0, 0);
ejecutar_macro_pinza(0);
estado_actual = st_espera_enlace;
}

// Estado de espera: mantenemos el robot completamente inmóvil hasta recibir la primera
// trama válida desde nuestra aplicación en MIT App Inventor
static void st_espera_enlace(void) {
id_estado_telemetria = 1;
manejar(0, 0, 0);
if (hubo_trama) {
estado_actual = st_manual;
}
}

// Estado de operación normal: trasladamos las consignas de avance y giro a la mezcla
// diferencial PWM, vigilando en todo momento si se corta el enlace Bluetooth o si
// nos acercamos demasiado a un obstáculo frontal
static void st_manual(void) {
id_estado_telemetria = 2;
uint8_t dist = ultrasonido_get_cm();

if (en_failsafe) {
estado_actual = st_failsafe;
} else if (anticolision_on && dist < DIST_FRENO_CM && v_cmd > 0) {
estado_actual = st_freno_prox;
} else {
manejar(v_cmd, w_cmd, modo_lento);
}
}

// Estado de asistencia anti-colisión: cuando el sensor ultrasónico detecta una pared
// o el estante a menos de 6 cm, anulamos únicamente el avance frontal (v_cmd = 0) para
// evitar penalizaciones por choque, pero conservamos la capacidad de girar o retroceder
static void st_freno_prox(void) {
id_estado_telemetria = 3;
uint8_t dist = ultrasonido_get_cm();

if (en_failsafe) {
estado_actual = st_failsafe;
} else if (!anticolision_on || dist >= DIST_FRENO_CM || v_cmd <= 0) {
estado_actual = st_manual;
} else {
manejar(0, w_cmd, modo_lento);
}
}

// Estado de seguridad por pérdida de señal: si dejan de llegar tramas por más de 300 ms,
// detenemos inmediatamente los motores de tracción. Decidimos mantener la última posición
// de la pinza sin abrirla para evitar soltar y perder la pieza en medio del recorrido
static void st_failsafe(void) {
id_estado_telemetria = 4;
manejar(0, 0, 0);
if (!en_failsafe) {
estado_actual = st_manual;
}
}

// Supervisamos periódicamente el tiempo transcurrido desde la última trama válida.
// Al usar resta de enteros sin signo de 32 bits evitamos cualquier problema cuando
// el contador millis() desborde tras varios días de funcionamiento
void failsafe_tarea(void) {
if (hubo_trama && (millis() - t_ultima_trama > FAILSAFE_MS)) {
en_failsafe = 1;
hubo_fs_flag = 1;
} else if (hubo_trama) {
en_failsafe = 0;
}
}


// Construimos y enviamos cada 200 ms la trama de telemetría hacia el teléfono celular.
// Incluimos la distancia medida por el sensor ultrasónico, el estado actual de la FSM
// y un byte de banderas empaquetadas con su correspondiente checksum XOR
void telemetria_tarea(void) {
uint8_t dist = ultrasonido_get_cm();
uint8_t obstaculo = (dist < DIST_FRENO_CM) ? 1 : 0;
uint8_t flags = (obstaculo << 0) | (anticolision_on << 1) | (hubo_fs_flag << 2);

uint8_t t[5];
t[0] = TRAMA_INICIO_TX;
t[1] = dist;
t[2] = id_estado_telemetria;
t[3] = flags;
t[4] = t[1] ^ t[2] ^ t[3];

for (uint8_t i = 0; i < 5; i++) {
uart_enviar(t[i]);
}
hubo_fs_flag = 0;

// Hacemos parpadear el LED de la placa cuando esperamos enlace o estamos en failsafe,
// y lo mantenemos apagado durante la conducción normal para diagnóstico rápido
if (estado_actual == st_espera_enlace || estado_actual == st_failsafe) {
PORTB ^= (1 << LED_PIN);
} else {
PORTB &= ~(1 << LED_PIN);
}
}

// Punto de entrada principal: inicializamos cada módulo de hardware por separado,
// fijamos el estado inicial de la máquina de estados, habilitamos las interrupciones
// globales y ejecutamos nuestro scheduler cooperativo sin utilizar retardos bloqueantes
int main(void) {
DDRB |= (1 << LED_PIN);

motores_init();
servo_init();
uart_init();
tick_init();
ultrasonido_init();

estado_actual = st_inicio;
sei();

uint32_t t10 = 0, t20 = 0, t200 = 0;

while (1) {
uint32_t ahora = millis();

// Cada 10 ms procesamos los bytes recibidos, verificamos el watchdog de enlace
// y ejecutamos la función correspondiente al estado actual del robot
if ((ahora - t10) >= 10) {
t10 += 10;
parser_tarea();
failsafe_tarea();
estado_actual();
}

// Cada 20 ms (50 Hz, coincidiendo con el período de los servos) actualizamos un
// paso de la rampa suave de movimiento para evitar golpes mecánicos y picos de corriente
if ((ahora - t20) >= 20) {
t20 += 20;
servo_tarea();
}

// Cada 200 ms disparamos una medición del sensor ultrasónico HC-SR04P y enviamos
// el paquete actualizado de telemetría a la aplicación móvil
if ((ahora - t200) >= 200) {
t200 += 200;
ultrasonido_tarea();
telemetria_tarea();
}
}
}
    
