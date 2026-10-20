# Android en Waydroid — guía de trabajo

Cómo compilar el APK de NOVA Studio para Android y probarlo en **Waydroid**
(el contenedor de Android que corre sobre Linux). Es el entorno de pruebas del
spike de audio: no hay móvil físico.

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
| `🔊 Spike: cargado … 44100 Hz, N canales` | el fichero se ha decodificado |
| `🎨 Onda: recibidos N picos` | los picos están en la caché y el renderizador los ha cogido |
| frames escritos > 0 y creciendo | el callback de Oboe está sirviendo audio |
| `🛑 Spike: streams cerrados` al salir | cierre limpio |
| **nada** de `SIGSEGV` / `Fatal signal` | sin crashes al cerrar |

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
| `Failed to get service waydroidplatform` | no hay compositor Wayland | usar la vía headless (§3.2) o entrar en una sesión Wayland |
| `Wayland socket ... doesn't exist` | sesión X11 | idem |
| `the session is already running` pero no hay adb | sesión zombi | `waydroid session stop && waydroid session start` |
| El APK no instala | ABI distinta | compilar con `--abi x86_64` |
| La app no encuentra el fichero | permisos de almacenamiento | copiarlo a `/sdcard/Android/data/com.novastudio.nova/files/` |
