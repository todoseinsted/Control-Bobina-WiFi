# Control Bobina WiFi — versión inicial (18 GPIO)

Abrir http://192.168.0.240/ desde la red local. Las credenciales permanecen en
`secrets.h`, excluido de Git.

## Uso

1. Marcar los GPIO que deben generar disparos.
2. Si hace falta, abrir **Ajustar frecuencia y tiempos**. Los campos están
   vinculados: `frecuencia = 1000000 / periodoUs`, `duty = 100 * Ton / periodo`,
   `pausa = periodo / 2 - Ton`.
3. Pulsar **Iniciar disparos**: aplica los parámetros y la selección e inicia.
4. Pulsar **Apagar disparos** para detener los 18 GPIO. Conserva la selección
   del formulario para poder volver a iniciar.

Los dos botones principales permanecen visibles al desplazarse por la página.
Dentro de los ajustes también se puede **Guardar cambios sin iniciar**.

Todas las salidas de un grupo comparten el mismo pulso. El grupo B va después
del A, con una pausa entre ambos. No hay frecuencia individual por GPIO ni
salida permanentemente en HIGH. Las casillas habilitan o deshabilitan PWM.

Se conserva exactamente el orden y los grupos del archivo original:

- A: 13, 14, 26, 33, 22, 19, 5, 16, 2.
- B: 27, 12, 25, 32, 23, 18, 17, 4, 15.

Parejas físicas: 26/25, 33/32, 22/23, 19/18, 5/17, 16/4, 2/15, 14/27, 13/12.
Conectores duplicados conectados al mismo GPIO no son independientes.

## Límites y comportamiento

- Frecuencia: 1–250 Hz; periodo: 4–1000 ms.
- Ton: 0,1–5 ms, máximo 45 % del periodo y pausa mínima 0,1 ms.
- Son límites del software: no certifican compatibilidad con la Hellux HE11421.
- Valores iniciales: 100 Hz, 30 %, Ton 3 ms; todas las salidas deshabilitadas.
- Reiniciar no recupera una configuración activa. No se guarda en flash.
- Una configuración válida aplicada detiene primero las salidas.
- Las configuraciones inválidas se rechazan sin alterar el estado anterior.
- La desconexión Wi-Fi detectada por el firmware detiene las salidas. Volver a
  conectarse no las inicia. Cerrar el navegador no equivale a detener.
- No hay protección por sensores implementada: tampoco estaba activa en el .ino original.
- Se mantienen IP fija y BSSID del punto de acceso previamente probado.

Temporizador GPTimer de 1 MHz con cuatro interrupciones por período (máximo
1000 por segundo). No hay tareas con espera activa. Cada banco de GPIO se
escribe en conjunto; los GPIO 32/33 requieren una segunda escritura. Se
programa cada transición desde el momento actual, sin comprimir pulsos para
recuperar retrasos. Existe un pequeño desfase de servicio de interrupción.
Un retraso detectado mayor a 500 us detiene las salidas y registra un fallo;
esto no reemplaza una protección eléctrica ni garantiza un Ton máximo físico.
La web muestra consignas, no mediciones. Verificar pulsos y polaridad en la
salida de los PC817 con instrumental antes de conectar la etapa de potencia:
un GPIO LOW no garantiza por sí solo una entrada de bobina desactivada.

## API local

- `GET /status`: configuración, estado, revisión, identificador de arranque,
  orden de GPIO, tiempo encendido, memoria libre, motivo del último reinicio
  y cantidad de fallos del temporizador.
- `POST /configure`: `periodUs`, `tonUs`, `mask`, `revision`, `bootId` (enteros
  decimales). Aplica la configuración y queda detenido.
- `POST /run`: los mismos parámetros; configura e inicia en una operación.
  Rechaza revisiones o identificadores de arranque antiguos.
- `POST /stop`: sin parámetros; siempre permite detener.

El bit 0 de `mask` corresponde a GPIO13 y los demás siguen el orden original,
no el número del GPIO. Los endpoints antiguos `/setRpm` y `/setFrequency` ya
no se usan.

## Estabilidad de la comunicación

La página mantiene una sola petición en curso, con espera máxima de 3 s.
Las consultas de estado no se superponen y se pausan con la pestaña oculta.
Apagar cancela la respuesta pendiente y agrupa clics repetidos en una orden.
Nunca se reintenta automáticamente un inicio; si se pierde su respuesta,
se intenta apagar. Un fallo de red se muestra como estado sin confirmar.
El apagado web depende de que la conexión funcione; no es un paro físico.

Los valores predeterminados y el orden de GPIO siguen siendo los originales.
La IP es 192.168.0.240. El BSSID se actualizó al punto de acceso de la PC
verificado el 14/10/2026: E4:C0:E2:91:5F:32 (canal 1). Si la PC cambia a otro
AP, comprobar la comunicación entre ambos AP antes de atribuirlo al ESP.

Prueba de regresión del cliente (Node.js): `node BancoBobina/tests/ui.cjs`.
La prueba simula demoras, respuestas perdidas y clics repetidos; no mide los
pulsos eléctricos. Los resultados de pruebas en la placa se guardan aparte.

## Verificación en la placa — 14/10/2026

- Compilación ESP32 Arduino 3.3.12: 946868 bytes de programa, 47592 bytes de RAM estática.
- Carga USB en COM3 con verificación de hash correcta.
- 295 peticiones en 75,19 s: validaciones, órdenes antiguas, 80 cambios,
  30 lecturas concurrentes, 60 inicios/reconfiguraciones y 30 apagados.
- 60 s sostenidos a 250 Hz / 45 % con los 18 GPIO habilitados, sin alimentación
  de 12 V en las bobinas. Sin reinicios ni fallos de temporizador registrados.
- Latencia HTTP máxima observada: 258 ms; media: 48 ms.
- Memoria libre al finalizar: 223696 bytes. No se observó agotamiento de memoria.
- Página servida por la placa idéntica al archivo local; pruebas de cliente aprobadas.
- Estado final: apagado, selección vacía, valores de 100 Hz / 30 %.

Detalle reproducible: `BancoBobina/tests/hardware-result.json`. Para repetir la prueba con
pulsos, desconectar primero los 12 V y ejecutar `python BancoBobina/tests/stress.py --pulses`.
Sin `--pulses`, sólo se prueban configuraciones detenidas. La prueba no certifica
la duración eléctrica de los pulsos ni el funcionamiento con bobinas energizadas.

## Instalación y archivos

Esta rama `main` conserva la primera versión: dos grupos de nueve GPIO alternados.
La variante de sólo GPIO22/23 simultáneos está en `codex/gpio22-23-simultaneos`.

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
