# Control Bobina WiFi — GPIO22 y GPIO23 simultáneos

Esta rama conserva el control web, la IP fija, la validación de parámetros y las
protecciones de comunicación de `main`, pero utiliza **únicamente GPIO22 y GPIO23**.
Ambos forman una sola salida lógica: se habilitan, suben y bajan juntos.
No se pueden seleccionar individualmente. Los demás GPIO no se configuran ni se
manejan como salidas por este programa (su estado de arranque depende del ESP32).

## Funcionamiento de los pulsos

Por cada período completo se genera un único pulso simultáneo:

```text
GPIO22: ___|‾‾‾|_______|‾‾‾|_______
GPIO23: ___|‾‾‾|_______|‾‾‾|_______
             Ton  Toff
```

- Frecuencia = 1000000 / período en microsegundos.
- Ciclo de trabajo = 100 × Ton / período.
- **Toff = período − Ton**. Ya no existe una pausa entre grupos A y B.
- Valores iniciales: **100 Hz**, período **10 ms**, **30 %**, Ton **3 ms**, Toff **7 ms**.
- Se mantienen los límites: 1–250 Hz, Ton de 0,1–5 ms y máximo 45 %.
- Arranque detenido y pareja deshabilitada; no se guardan ajustes en flash.

El temporizador GPTimer opera a 1 MHz y genera dos interrupciones por período
(máximo 500 por segundo). Una escritura de registro activa los bits 22 y 23 a
la vez; otra escritura los apaga. No hay espera activa ni alternancia entre ellos.
Los tiempos indicados son consignas: existe latencia de atención de interrupciones.
Un retraso detectado mayor a 500 us detiene el generador y registra un fallo.
Esto no garantiza un Ton máximo físico durante cualquier avería.

## Uso de la página

1. Abrir http://192.168.0.240/ desde la red local.
2. Marcar **Habilitar ambos** en la fila GPIO22 + GPIO23.
3. Ajustar frecuencia, período, porcentaje, Ton o tiempo apagado si hace falta.
   Los campos se recalculan entre sí.
4. Pulsar **Iniciar disparos**. Si ya están activos, aplicar los nuevos parámetros
   detiene el pulso previo e inicia una nueva secuencia simultánea.
5. Pulsar **Apagar disparos** para llevar ambos GPIO a LOW.

**Guardar cambios sin iniciar** configura y deja detenido. **Restablecer 100 Hz /
30 %** prepara esos valores en el formulario. **Recuperar valores de la placa**
lee la configuración actual. Recargar con Ctrl + F5 después de cambiar de firmware.

El cliente mantiene una sola petición en curso, con tiempo de espera de 3 s.
Agrupa clics de apagado y cancela respuestas antiguas. Si un inicio pierde su
respuesta, intenta apagar en lugar de repetir el inicio. La reconexión Wi-Fi no
reinicia los pulsos; la desconexión detectada los detiene. Cerrar la página no
los detiene. El botón web depende de la red y no sustituye un paro físico.

## API

- `GET /status`: parámetros, `pins: [22,23]`, estado, revisión, `bootId`, memoria,
  tiempo encendido, causa de reinicio y fallos del temporizador.
- `POST /configure`: configura y deja detenido.
- `POST /run`: configura e inicia en una sola operación.
- `POST /stop`: apaga sin requerir parámetros.

`/configure` y `/run` requieren enteros decimales `periodUs`, `tonUs`, `mask`,
`revision` y `bootId`. Los dos últimos deben corresponder al estado actual.
**mask=0** deshabilita ambos y **mask=3** selecciona ambos (bit 0=22, bit 1=23).
Las máscaras 1, 2 y cualquier otro valor se rechazan. `/run` rechaza mask=0.
Una orden antigua devuelve HTTP 409; parámetros inválidos devuelven HTTP 400
sin alterar el estado. No usar máscaras de la versión anterior de 18 GPIO.

## Validación de esta rama

Las pruebas del cliente verifican la selección conjunta, la pausa de 7 ms,
la serialización de órdenes, apagados repetidos y recuperación tras errores.
La compilación se comprueba con el core ESP32 3.3.12.
**Esta variante no fue cargada ni medida físicamente en el ESP32.**
`BancoBobina/tests/hardware-result-main-18gpio.json` contiene únicamente los
resultados históricos de `main`, no una prueba de esta variante.
El script `tests/stress.py` se adaptó a máscaras 0/3. Ejecutarlo con `--pulses`
activa ambos GPIO y requiere desconectar previamente la alimentación de bobinas.
Los límites de software no certifican compatibilidad eléctrica con la Hellux.

## Instalación y archivos

Esta rama es `codex/gpio22-23-simultaneos`. La primera versión de 18 GPIO
alternados permanece disponible en `main`.

1. Instalar Arduino IDE con el paquete **esp32 de Espressif**, versión probada 3.3.12.
2. Abrir `BancoBobina/BancoBobina.ino` y seleccionar **ESP32 Dev Module**.
3. Copiar `BancoBobina/secrets.example.h` como `BancoBobina/secrets.h` y completar
   nombre y contraseña Wi-Fi. Este archivo queda excluido de Git.
4. Revisar IP, máscara, gateway, DNS, BSSID y canal en el `.ino`. El BSSID incluido
   pertenece a la instalación original; debe cambiarse para otro punto de acceso.
   Para conectar por SSID sin fijar AP, usar `WiFi.begin(ssid, password)`.
5. Compilar y cargar por USB; monitor serie a **115200 baudios**.
6. Abrir la dirección configurada desde un equipo con acceso a esa red.

La IP estática requiere una dirección libre del mismo segmento. Para evitar
conflictos futuros, reservarla por MAC en el router o excluirla del rango DHCP.
El programa no configura automáticamente el router.

- `BancoBobina.ino`: Wi-Fi, servidor HTTP, temporizador y control de salidas.
- `control.h`: orden de GPIO y validación de parámetros.
- `web_page.h`: página HTML/CSS/JavaScript almacenada dentro del ESP32.
- `tests/ui.cjs`: pruebas del cliente con Node.js, sin hardware.
- `tests/stress.py`: pruebas HTTP sobre la placa; `--pulses` activa salidas.

Compilación por consola desde la raíz del repositorio:

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 BancoBobina
node BancoBobina/tests/ui.cjs
```

No se necesitan bibliotecas externas al core ESP32. La API no tiene autenticación:
se utiliza en una red local de confianza, sin publicar el puerto en Internet.
El PC817 no limita por sí mismo la tensión de entrada a la bobina; verificar la
interfaz eléctrica antes de energizarla. La selección de GPIO no valida el cableado.
