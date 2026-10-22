# NOVA Studio — instrucciones para el agente

Este archivo es la guía **única** de cómo se construye y se prueba este repo.
Léelo antes de tocar nada: casi todos los errores que se cometen aquí vienen de
suponer una arquitectura que no es la real.

Todo el texto del repo (código, comentarios, tests y documentación) va en
**español**. Los commits también, con el prefijo conventional commits
(`feat(...)`, `fix(...)`, `docs(...)`, `build(...)`, `chore(...)`, `test(...)`).

---

## 1. Cómo se compila de verdad

**El motor de audio lo compila CMake, NO Gradle.** Son dos etapas distintas:

```
qt-cmake (CMake)
    └─> libnova_ui_qt_<abi>.so        ← aquí vive TODO el C++, incluido el motor
            │
            ▼
androiddeployqt (dentro del build de Qt)
    └─> build_android/android-build/   ← staging: proyecto Gradle + .so + jars de Qt
            │
            ▼
Gradle  ──> app-release.apk / app-debug.apk   ← SOLO empaqueta, no compila C++
```

Por eso `android/app/build.gradle` **no** tiene bloque `externalNativeBuild`: es
deliberado. Está escrito en el propio fichero
(`android/app/build.gradle`, comentario "NOTE: no externalNativeBuild here").

Las piezas del motor (todas en `src/audio/`, en C++ puro y testeables en el host
**sin Android**):

| Capa | Fichero | Qué hace |
|---|---|---|
| Fachada | `src/audio/NovaAudioDecoder.*` | WAV o MP3, detectado **por contenido**, no por extensión |
| Lectores | `src/audio/NovaWavReader.*`, `src/audio/NovaMp3Decoder.*` | WAV propio + minimp3 (vendorizado en `third_party/minimp3/`) |
| Remuestreo | `src/audio/NovaResampler.*` | Lineal, posición fraccionaria persistente (44,1 ↔ 48 kHz) |
| Reproducción | `src/audio/NovaAudioPlayer.*` | PCM → frames de salida, posición, fin de fichero |
| Picos | `src/audio/NovaAudioPeaks.*` | PCM → `PeakData{min,max,rms}` con el SIMD que ya existía |
| Salida | `src/core/platform/NovaAndroidAudioEngine.*` | Oboe; el callback pide frames al reproductor |
| Orquestación | `src/core/platform/NovaSpikeController.*` | Publica picos en `NovaWaveformCache` y mueve el playhead |

Oboe aparece **solo en la última capa**. Todo lo demás se prueba en Linux.

---

## 2. Comandos canónicos

```bash
# Compilar y probar el escritorio (Linux)
cmake -S . -B build && cmake --build build -j$(nproc)
cd build && ctest --output-on-failure

# Compilar un APK de Android (el ÚNICO camino válido)
./build_android.sh --abi arm64-v8a          # teléfono real
./build_android.sh --abi x86_64             # imagen de Waydroid
./build_android.sh --abi arm64-v8a --debug  # APK Debug

# Verificación completa (lo que hay que ejecutar antes de abrir un PR)
./scripts/verify.sh            # contratos + ctest (necesita el árbol de Ardour)
./scripts/verify_contracts.sh  # solo contratos (Python: corre en cualquier sitio)
```

**No lances Gradle a mano** y **no entres en `android/` para compilar**: el
staging de Gradle solo existe dentro de `build_android/android-build/` y lo
genera `androiddeployqt`. Si necesitas tocar `android/app/build.gradle`, se toca
**el del repo** (es el que se copia al staging).

---

## 3. Trampas conocidas (no caigas en ellas)

1. **`android/` NO es un proyecto Gradle autónomo.** Le faltan `gradlew`,
   `gradle/wrapper/`, `app/libs/` (los jars de Qt) y `libs/` (las `.so` que
   Gradle empaqueta). Es solo la plantilla que `androiddeployqt` copia al
   staging. Compilar con `./gradlew` desde ahí falla siempre.

2. **Oboe se trae con `FetchContent`, no con `find_package`.**
   En `CMakeLists.txt` está `FetchContent_Declare(oboe GIT_REPOSITORY … GIT_TAG 1.7.0)`.
   **No existe ningún `oboeConfig.cmake`** en el sistema, así que
   `find_package(oboe 1.7 REQUIRED)` **falla siempre** con
   *"Could not find a package configuration file provided by oboe"*. Ese error
   ha costado una hora de depuración: no es un problema del motor, es una
   llamada a CMake que no puede funcionar.

3. **El target de CMake de Oboe es `oboe`, no `oboe::oboe`.** El alias
   `oboe::oboe` existe, pero el que usa el `CMakeLists.txt` raíz es `oboe`.

4. **El nombre de arquitectura de aqtinstall para Android es `android_<abi>`.**
   Con `aqtinstall`:
   ```bash
   aqt install-qt linux desktop 6.8.3 linux_gcc_64      -O $HOME/Qt  # trae androiddeployqt
   aqt install-qt linux android 6.8.3 android_arm64_v8a -O $HOME/Qt  # trae qt-cmake y las libs
   ```
   `aqt install-qt linux android 6.8.3 linux_gcc_64` **falla** (404): para
   Android el arch es el nombre del subdirectorio por arquitectura. El repo
   espera el kit en `$HOME/Qt/6.8.3/android_<abi>` (con guiones bajos).

5. **Los APKs y el `.so` del staging pueden estar viejos.** El `.so` del staging
   lo regenera `androiddeployqt` en cada corrida de `build_android.sh`. Si
   empaquetas un APK sin re-ejecutar el script, sale el motor de la vez
   anterior. Comprueba la fecha antes de dar por bueno un APK.

6. **El motor no se instancia desde `NovaSessionManager`.** En Android, el
   `ARDOUR::AudioEngine` es el **stub vacío** de
   `src/core/platform/NovaAndroidStubs.h`. Quien instancia el motor de Oboe es
   `NovaSpikeController`. Quitar los stubs es trabajo de la Fase 1, no de un
   arreglo puntual.

---

## 4. Reglas del repo

- **El escritorio no se toca.** `src/`, `qml/` (fuera de `qml/spike/`) y el
  enlace con Ardour son de la app de escritorio. Un cambio de móvil no puede
  alterar el comportamiento en Linux.
- **`ctest` tiene que seguir en verde.** Son 10 tests: `project_policy`,
  `simd_peaks`, `audio_decoder`, `audio_player`, `audio_peaks`, `audio_loader`,
  `qml_contract`, `android_build_script`, `spike_contract`, `android_pipeline`.
- **Los tests de contrato leen el código como texto.** `tests/check_*.py`
  comprueban invariantes del repo leyendo ficheros. Si rompes una invariante,
  ese test es el que te lo dice. `tests/check_android_pipeline.py` es el que
  vigila el camino de build de Android: si añades un `externalNativeBuild`,
  si usas `find_package(oboe)` o si vuelve el CMakeLists obsoleto, falla.
- **`third_party/` es código ajeno: no se modifica.** `minimp3` va vendorizado
  tal cual, con su README de origen y licencia.
- **Los stubs de Android (`NovaAndroidStubs.h`) son para Windows y para lo que
  aún no está implementado.** No los amplíes para tapar un hueco: se
  documenta y se deja el hueco visible.
- **No se reescribe historia.** Ni `git push --force`, ni `git reset --hard` en
  una rama compartida, ni amends de commits ya publicados.

---

## 5. Antes de abrir el Pull Request

```bash
./scripts/verify.sh
```

Ese script corre `ctest` (los tests de C++) **y** los contratos en Python
(invariantes del repo). Si falla algo, se arregla antes de subir nada. No
digas "debería funcionar": el script es el que dice si funciona.

`scripts/verify_contracts.sh` es solo la capa de contratos (Python puro): es
la que corre en CI y en la verificación post-agente del workflow de OpenCode,
donde no hay Qt ni el árbol de Ardour.

Si el cambio toca Android, añade también el motivo en el PR: **qué invariante
se mantiene** y **cómo se comprueba**. El revisor no va a reconstruir el
pipeline de build desde cero.

---

## 6. Idioma y tono

- Código, comentarios, nombres de test, documentación y mensajes de error: **español**.
- Los identificadores que ya existen en inglés (`renderFrames`, `loadFile`) se
  respetan; los nuevos, en español, como el resto.
- Commits en español, con prefijo de conventional commits.
- Cuando algo **no** se pueda verificar en el entorno (por ejemplo, un
  dispositivo Android real), dilo explícitamente en el PR en vez de darlo por
  bueno. La honestidad sobre los límites vale más que un "listo".
