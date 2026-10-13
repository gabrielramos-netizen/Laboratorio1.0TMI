import serial
import sys
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

PUERTO_COM = 'COM3'  # Cambia por tu puerto exacto si es diferente
BAUD_RATE = 9600

try:
    ser = serial.Serial(PUERTO_COM, BAUD_RATE, timeout=0.1)
    ser.reset_input_buffer()  # Limpiar buffer residual al conectar
    print(f"Conectado exitosamente a {PUERTO_COM}")
except Exception as e:
    print(f"[ERROR] No se pudo abrir {PUERTO_COM}: {e}")
    sys.exit()

max_puntos = 100
historico_sp = []
historico_ldr = []
historico_pot = []
historico_pwm = []
ultima_direccion = "STOP"

fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 8), sharex=True)
fig.canvas.manager.set_window_title('Telemetría en Tiempo Real - Control de Persiana')

def actualizar_grafico(frame):
    global historico_sp, historico_ldr, historico_pot, historico_pwm, ultima_direccion

    hay_datos_nuevos = False

    # Drenar todo el búfer de entrada acumulado para ponernos al día instantáneamente
    while ser.in_waiting > 0:
        try:
            linea = ser.readline().decode('utf-8', errors='ignore').strip()
            partes = linea.split(',')

            if len(partes) == 5:
                sp = int(partes[0])
                ldr = int(partes[1])
                pot = int(partes[2])
                pwm = int(partes[3])
                ultima_direccion = partes[4]

                historico_sp.append(sp)
                historico_ldr.append(ldr)
                historico_pot.append(pot)
                historico_pwm.append(pwm)
                hay_datos_nuevos = True
        except Exception:
            pass

    # Redibujar solo cuando se hayan recibido muestras nuevas
    if hay_datos_nuevos:
        historico_sp = historico_sp[-max_puntos:]
        historico_ldr = historico_ldr[-max_puntos:]
        historico_pot = historico_pot[-max_puntos:]
        historico_pwm = historico_pwm[-max_puntos:]

        # Gráfico superior (Sensores y Setpoint)
        ax1.clear()
        ax1.plot(historico_sp, label='Setpoint', color='red', linestyle='--')
        ax1.plot(historico_ldr, label='LDR (Luz Real)', color='orange')
        ax1.plot(historico_pot, label='Potenciómetro', color='blue')
        ax1.set_ylim(-20, 1050)  # Límite fijo para evitar deformaciones
        ax1.set_ylabel('Valor ADC (0-1023)')
        ax1.set_title(f'Estado del Sistema | Dirección: {ultima_direccion}')
        ax1.legend(loc='upper left')
        ax1.grid(True)

        # Gráfico inferior (PWM)
        ax2.clear()
        ax2.plot(historico_pwm, label='PWM Motor', color='green')
        ax2.set_ylim(-10, 200)  # Límite fijo para mantener estabilidad
        ax2.set_ylabel('PWM (0-180)')
        ax2.set_xlabel('Muestras')
        ax2.legend(loc='upper left')
        ax2.grid(True)

# Refresco acelerado a 30 ms para suavidad completa
ani = FuncAnimation(fig, actualizar_grafico, interval=30, cache_frame_data=False)

plt.tight_layout()
plt.show()