# NOVA Studio — Qt 6 / QML UI

Interfaz de usuario de NOVA Studio reescrita en **Qt 6 + QML** (antes: Qt5 Widgets).
Motor de audio, transporte, sesión y renderizado de waveforms en C++20; skins y
componentes de interfaz en QML modular.

Esta carpeta es un **repositorio independiente**: no necesita el historial del
monorepo de NOVA-STUDIO para compilar en Android y Windows.

---

## Estructura

| Ruta | Contenido |
|------|-----------|
| `main.cpp` | Punto de entrada, bootstrap de `QQmlApplicationEngine` |
| `src/core/` | Núcleo: sesión, transporte, grabación, dispositivos de audio, logging |
| `src/core/platform/` | Abstracción de plataforma (Android/Linux/Windows) |
| `src/models/` | Modelos C++: pistas, regiones, caché de peaks |
| `src/views/` | Renderizado de waveform y clips (OpenGL / QML `QQuickItem`) |
| `qml/` | UI en QML: `screens/`, `components/`, `primitives/`, `theme/` |
| `qml/skins/` | Skins: `bandlab/`, `logic/`, `reaper/`, `draft/` |
| `android/` | Módulo Gradle, manifest, servicio de audio en primer plano |
| `cmake/` | Helpers de CMake (`NovaQml.cmake`, toolchain MinGW) |
| `tools/qml2svg` | Exporta iconos `PathSvg` de QML a SVG estático |

---

## Dependencias por plataforma

### Android (autocontenido)
- Android SDK 35, NDK 26.1.10909125, JDK 17
- Qt 6.8.3 para `android_arm64_v8a` / `android_x86_64`
- Oboe 1.7.0 (se descarga vía `FetchContent`)
- OpenSSL 3.1.8 vendorizado en `third_party/android_openssl` (Apache-2.0),
  necesario para HTTPS en el actualizador

### Windows (autocontenido)
- Qt 6 para MSVC, toolchain MinGW-w64 (`cmake/mingw-w64-x86_64.cmake`)

### Linux desktop (requiere NOVA-STUDIO core)
El target de escritorio enlaza contra las librerías compartidas de **Ardour
Core** (`libs/ardour`, `libs/pbd`, `libs/temporal`, `libs/evoral`, `libs/midi++2`,
`libs/fst`, `libs/tk/suil`, `libs/audiographer`) y usa sus cabeceras.

CMake las resuelve en rutas **relativas al padre**:

```
ARDOUR_BUILD_DIR = <este_repo>/../build
ARDOUR_SRC_DIR   = <este_repo>/..
```

Por tanto, para compilar en Linux hace falta un checkout del core de
NOVA-STUDIO (fork de Ardour) como repositorio hermano:

```bash
git clone https://github.com/thelandy03-boop/NOVA-STUDIO.git
# este repo debe quedar como NOVA-STUDIO/nova_ui_qt
```

Android y Windows **no** dependen del core: usan stubs de plataforma
(`src/core/platform/NovaAndroidStubs.cpp`).

---

## Compilación

### Android
```bash
./build_android.sh          # automatizado: staging + gradle + APK
# o manualmente:
cd android && ./gradlew assembleDebug
```

### Windows
```bash
./build_windows.sh
```

### Linux desktop
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

---

## Scripts

| Script | Función |
|--------|---------|
| `build_android.sh` | Build completo de APK (staging Qt → CMake → Gradle) |
| `build_apk.sh` | Atajo para compilar un APK con ABI/tipo configurables |
| `publish_release.sh` | Incrementa versión, compila APKs y publica la release en GitHub |
| `build_windows.sh` | Cross-compile a Windows x64 con MinGW |
| `run.sh` | Lanza la app en Linux con PipeWire-JACK y rutas de libs de Ardour |
| `release.sh` | Versiona, tagea y publica release en GitHub (se ejecuta desde la raíz del monorepo) |
| `tools/qml2svg` | Exporta iconos vectoriales de QML a SVG |

`ANDROID_BUILD.md` documenta el setup completo de Android.

### Compilar un APK

```bash
./build_apk.sh --abi x86_64 --debug     # Waydroid / emulador
./build_apk.sh --abi arm64-v8a --release # teléfonos
```

El APK queda en `apks/` (`NovaStudio-<abi>[-debug].apk`).

### Publicar una actualización

```bash
./publish_release.sh patch              # 1.0.1 → 1.0.2 (también minor|major)
./publish_release.sh minor --no-x86     # solo APK arm64
```

El script sella la versión en `NovaUpdateChecker.h` y `build.gradle`, compila
los APKs, crea el tag `vX.Y.Z`, lo sube y publica la release con `gh`. La app
Android compara su versión contra `releases/latest` y bloquea la UI hasta
actualizar; el APK se elige según el ABI del dispositivo.

---

## Nota sobre la licencia

El código deriva de Ardour y se distribuye bajo **GPL v2 o posterior**.
Ver [LICENSE](LICENSE). El core de NOVA-STUDIO (fork de Ardour) vive en el
repositorio hermano con su propia licencia.
