# Android en Waydroid — guía de trabajo

Cómo compilar el APK de NOVA Studio para Android y probarlo en **Waydroid**
(el contenedor de Android que corre sobre Linux). Es el entorno de pruebas del
spike de audio: no hay móvil físico.

> ⚠️ **En esta máquina el contenedor no llega a ser utilizable**: Android
> dentro de LXC se queda sin `netd` (sin DHCP, sin IPv4) y `adbd` no abre su
> puerto, así que **no hay `adb`** y la verificación en el dispositivo está
> bloqueada. El diagnóstico está en §3.3; el código del spike está completo y
> verificado por tests, pero **la Task 6 del plan sigue abierta**.

---

## 1. Requisitos (ya instalados en esta máquina)

| Pieza | Estado | Ruta |
|---|---|---|
| Qt para Android | 6.8.3 (arm64_v8a **y** x86_64) | `~/Qt/6.8.3/` |
| NDK | 26.1.10909125 | `~/Android/Sdk/ndk/` |
| JDK | 17 o 21 | `JAVA_HOME` en `build_android.sh` |
| Waydroid | imagen `waydroid_x86_64` | `/var/lib/waydroid/images/` |

El APK de producción es **arm64-v8a** (móviles reales). Para Waydroid hace falta
**x86_64**, porque la imagen del contenedor es x86_64.

## 2. Compilar

```bash
cd ~/Descargas/NOVA-STUDIO/nova_ui_qt
./build_android.sh --abi x86_64      # para el emulador
./build_android.sh --abi arm64-v8a   # para un móvil real
```

El script regenera `build_android/` si su cache de CMake pertenece a otro ABI,
así que se pueden alternar sin limpiar nada. Salida:

```
apks/NovaStudio-x86_64.apk
apks/NovaStudio-arm64-v8a.apk
```

Comprobar que el APK trae el motor y no solo el código:

```bash
unzip -l apks/NovaStudio-x86_64.apk | grep -c "lib/x86_64/.*\.so$"   # ~80
unzip -l apks/NovaStudio-x86_64.apk | grep libnova_ui_qt
```

## 3. Levantar el emulador

### 3.1 Con interfaz gráfica (necesita una sesión **Wayland**)

La UI de Waydroid (SurfaceFlinger/GLES) **solo funciona sobre Wayland**. En una
sesión X11 pura no arranca, y `cage -- waydroid show-full-ui` falla con
*"Wayland socket ... doesn't exist"*.

```bash
sudo systemctl start waydroid-container
waydroid session start        # levanta contenedor + red + UI
cage -- waydroid show-full-ui # la interfaz (dentro de un compositor "cage")
```

### 3.2 Sin interfaz gráfica (funciona sobre X11)

El contenedor puede correr **sin UI**: es un Android headless al que se entra por
adb. Sirve para verificar audio, logs y hasta capturas de pantalla.

```bash
sudo systemctl restart waydroid-container   # levanta contenedor + red (root)
adb connect 192.168.240.2:5555              # el contenedor queda en esa IP
adb devices                                  # debe listar el dispositivo
```

> **Ojo:** `waydroid session stop` **tira la red** (borra el veth `waydroid0`).
> Si se para la sesión, la siguiente puesta en marcha debe ser con root:
> `sudo systemctl restart waydroid-container`.

### 3.3 ⚠️ Bloqueo conocido: Android dentro de LXC (sin `netd`)

En esta máquina el contenedor de Waydroid **arranca** (Android llega a
`system_server`) pero **no hay `adb`**:

1. El contenedor es un Android **dentro de un LXC**, y su `init` no levanta el
   servicio `netd` (el que hace la configuración de red por DHCP).
2. Sin `netd` no hay **DHCP** → el contenedor se queda **sin IPv4** (solo el
   `127.0.0.1` de la loopback).
3. Sin IPv4, `adbd` **no abre su puerto TCP** (5555): el transporte de `adb`
   no llega a escuchar.
4. Por eso `adb devices` **no lista nada** y no se puede instalar ni mirar el
   logcat, aunque el contenedor esté vivo.

Se intentó asignar la IP a mano
(`nsenter -t <pid init> -n ip addr add 192.168.240.2/24 dev eth0`) y **sigue
sin funcionar**: sin `netd`, `adbd` no abre su puerto.

**Qué significa para el spike:** la verificación funcional en el dispositivo
(la Task 6 del plan) **no se puede hacer en este entorno**: es un bloqueo del
**entorno**, no del código. El código del spike está completo y verificado
por tests (`ctest`), pero **la Task 6 NO está cerrada**: hace falta un
entorno donde `adb` funcione (un móvil de verdad, o un host con Waydroid
funcionando) para confirmar el sonido y el playhead.

## 4. Instalar y lanzar

```bash
adb install -r apks/NovaStudio-x86_64.apk
adb shell am start -n com.novastudio.nova/com.novastudio.nova.MainActivity
```

## 5. Poner un fichero de audio

La app solo puede leer sin permisos especiales **su propia carpeta externa**
(scoped storage). `/sdcard/Music` requiere el permiso `MANAGE_EXTERNAL_STORAGE`,
que en una instalación limpia no está concedido:

```bash
adb push tests/fixtures/tono-1s-44100-mono.mp3 \
    /sdcard/Android/data/com.novastudio.nova/files/spike.mp3
```

Esa es la ruta por defecto del campo de texto de la página del spike.

## 6. Qué mirar

```bash
adb logcat -c
adb shell am start -n com.novastudio.nova/com.novastudio.nova.MainActivity
sleep 6
adb logcat -d | grep -E "Spike|Nova|🔊|🛑|🎨|📈" | tail -40
```

Lo que debe aparecer:

| Línea | Significado |
|---|---|
| `🔊 Spike: cargando /sdcard/…` | se ha pedido el fichero (se decodifica y se calculan los picos en un hilo aparte) |
| `🔊 Spike: cargado N canales, X s a Y Hz` | el audio está en el motor, remuestreado a la **tasa real** del stream (Y) |
| `🎨 Onda: recibidos N picos` | los picos están en la caché y el renderizador los ha cogido |
| `🔊 Spike: reproduciendo N picos` | el fichero está en marcha |
| `🔊 Spike: fotogramas escritos N posición X s` | el callback de Oboe está sirviendo audio: los fotogramas escritos crecen (una vez por segundo) |
| `🛑 Spike: streams cerrados` al salir | cierre limpio |
| **nada** de `SIGSEGV` / `Fatal signal` | sin crashes al cerrar |

El criterio objetivo de la verificación es la línea de los **fotogramas
escritos**: tiene que salir con un número **mayor que 0 y creciendo** (la
posición también). Si el número se queda en 0, el callback de Oboe no está
sirviendo audio.

Captura de pantalla (para ver la onda y el cabezal):

```bash
adb exec-out screencap -p > /tmp/spike.png
```

## 7. Qué **no** se puede verificar en Waydroid

Waydroid es un Android en un contenedor: sirve para lógica, decodificación,
interfaz y formato, **pero no reproduce el hardware de un móvil**.

| Qué | Por qué | Cómo se comprueba |
|---|---|---|
| **Latencia real** | el HAL es virtual sobre el audio del host (PipeWire a 48 kHz) | en un móvil de verdad |
| **AAudio / baja latencia** | no hay HAL de audio real; Oboe cae a OpenSL ES o a un HAL emulado | en un móvil de verdad |
| **Micrófono** | depende de que el host exponga su entrada | en un móvil de verdad |
| **Gestos multitáctiles** (pinza de zoom) | el táctil es el ratón | en un móvil de verdad |
| **Doze / background** | no hay gestión de energía real | en un móvil de verdad |

Antes de publicar a usuarios hay que hacer una **revisión en hardware**.

## 8. Problemas frecuentes

| Síntoma | Causa | Solución |
|---|---|---|
| `adb devices` vacío | la red del contenedor está caída | `sudo systemctl restart waydroid-container` y `adb connect 192.168.240.2:5555` |
| `adb devices` vacío **aunque el contenedor esté vivo** | Android dentro de LXC sin `netd` → sin DHCP → sin IPv4 → `adbd` no abre su puerto (§3.3) | **no se puede arreglar desde el host**: hace falta un entorno con `adb` funcional (un móvil de verdad) |
| `Failed to get service waydroidplatform` | no hay compositor Wayland | usar la vía headless (§3.2) o entrar en una sesión Wayland |
| `Wayland socket ... doesn't exist` | sesión X11 | idem |
| `the session is already running` pero no hay adb | sesión zombi | `waydroid session stop && waydroid session start` |
| El APK no instala | ABI distinta | compilar con `--abi x86_64` |
| La app no encuentra el fichero | permisos de almacenamiento | copiarlo a `/sdcard/Android/data/com.novastudio.nova/files/` |
