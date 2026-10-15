#IMPORTAMOS LAS LIBRERIAS QUE PRECISAMOS 
import sys
import time
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from matplotlib.widgets import TextBox
import serial
import serial.tools.list_ports

PUERTO_SERIE = 'COM6'
BAUDRATE = 9600

# Abrimos la conexión serie
try:
  ser = serial.Serial(PUERTO_SERIE, BAUDRATE, timeout=1)
  time.sleep(2)
  print(f'>> Conectado con éxito a {PUERTO_SERIE}')
except Exception as e:
  print(f'>> Error al abrir el puerto: {e}')
  sys.exit()

#definimos nuestras variables: 
tiempos = []
temperaturas = []
calefactores = []
ventiladores_pct = []
pm_mins = []
pm_maxs = []

tiempo_inicio = time.time()

# Ajustamos el espacio inferior para el cuadro de texto
fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 8), sharex=True)
fig.canvas.manager.set_window_title(
    'Control de Temperatura'
)
plt.subplots_adjust(bottom=0.18)


def actualizar(frame):
  while ser.in_waiting > 0:
    try:
      linea = ser.readline().decode('utf-8', errors='ignore').strip()

      if linea.startswith('TEMP:'):
        partes = linea.split(',')
        temp = int(partes[0].split(':')[1])
        cal = int(partes[1].split(':')[1])
        pwm = int(partes[2].split(':')[1])
        p_min = int(partes[3].split(':')[1])
        p_max = int(partes[4].split(':')[1])

        t_relativo = round(time.time() - tiempo_inicio, 1)

        tiempos.append(t_relativo)
        temperaturas.append(temp)
        calefactores.append(cal)
        ventiladores_pct.append((pwm / 255.0) * 100)
        pm_mins.append(p_min)
        pm_maxs.append(p_max)

        if len(tiempos) > 40:
          tiempos.pop(0)
          temperaturas.pop(0)
          calefactores.pop(0)
          ventiladores_pct.pop(0)
          pm_mins.pop(0)
          pm_maxs.pop(0)

        # --- PANEL 1 ---
        ax1.clear()
        ax1.plot(
            tiempos,
            temperaturas,
            'r-o',
            label='Temperatura Actual (°C)',
            linewidth=2,
        )
        ax1.fill_between(
            tiempos,
            pm_mins,
            pm_maxs,
            color='green',
            alpha=0.2,
            label='Rango Ideal',
        )
        ax1.plot(tiempos, pm_mins, 'g--', alpha=0.6)
        ax1.plot(tiempos, pm_maxs, 'g--', alpha=0.6)

        ax1.set_ylabel('Temperatura (°C)')
        ax1.set_title('Monitoreo Térmico en Tiempo Real')
        ax1.grid(True, linestyle='--', alpha=0.5)
        ax1.legend(loc='upper left')

        # PANEL 2 
        ax2.clear()
        ax2.step(
            tiempos,
            calefactores,
            'orange',
            where='post',
            label='Calefactor (1=ON, 0=OFF)',
            linewidth=2,
        )
        ax2.plot(
            tiempos,
            ventiladores_pct,
            'b-s',
            label='Ventilador (%)',
            linewidth=1.5,
        )

        ax2.set_xlabel('Tiempo (Segundos)')
        ax2.set_ylabel('Acciones del Sistema')
        ax2.set_ylim(-10, 110)
        ax2.grid(True, linestyle='--', alpha=0.5)
        ax2.legend(loc='upper left')

        fig.canvas.draw_idle()

      elif '>>' in linea or 'MENU' in linea:
        print(f'[AVR Respuesta]: {linea}')

    except Exception as e:
      print(f'Error de parseo: {e}')


#CUADRO DE TEXTO PARA CAMBIAR PUNTO MEDIO
ax_box = plt.axes([0.35, 0.04, 0.3, 0.05])
text_box = TextBox(ax_box, 'Nuevo PM Min (2 dígitos): ', initial='')


#Esta es una función que permite modificar el punto medio 
def cambiar_punto_medio(valor):
  valor = valor.strip()
  # Valida que sean exactamente 2 dígitos numéricos
  if len(valor) == 2 and valor.isdigit():
    # Hay que enviar 'm' seguido de los 2 dígitos (ej: 'm18')
    trama_envio = f'm{valor}'
    ser.write(trama_envio.encode('utf-8'))
    print(f">> Enviado al AVR: '{trama_envio}'")
    text_box.set_val('')  # Limpia la casilla
  else:
    print('>> Error: Debes ingresar exactamente 2 números (ejemplo: 16, 20, 25)')


text_box.on_submit(cambiar_punto_medio)

ani = FuncAnimation(fig, actualizar, interval=500, cache_frame_data=False)

plt.show()