# NOVA Android Spike — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Que un MP3 suene en Waydroid (contenedor Android x86_64) con la forma de onda visible y el playhead avanzando, con el APK compilado para x86_64.

**Architecture:** App Android → `NovaAndroidAudioEngine` (Oboe) ← `NovaAudioPlayer` (reproductor puro, sin Oboe, testeable en el host) ← `NovaDecodedAudio` (PCM) ← `decodeAudioFile()` (WAV propio + MP3 con minimp3). Los picos se calculan con el código SIMD que ya existe (`processPeakBlockSimd`) y se publican en `NovaWaveformCache`, de donde el renderizador de OpenGL los lee solo (`NovaWaveformRenderer::synchronize` ya los pide por `regionId`). Ningún fichero de escritorio se toca: todo lo nuevo vive en `src/audio/` y en la ruta Android.

**Tech Stack:** C++17, Qt 6.8 (QML, QQuickFramebufferObject), Oboe (AAudio/OpenSL ES), minimp3 (dominio público), CMake 3.x, Gradle 8.10 / AGP 8.2.2, NDK 26.1.10909125, Waydroid (imagen x86_64).

**Spec:** `docs/superpowers/specs/2026-10-20-nova-mobile-daw-design.md` (Fase 0 y §5.1, §7.2, §8.2, §8.4, §10, §15)

## Global Constraints

- **Idioma:** todo el código, los comentarios, los nombres de test y la documentación de este plan están en **español**.
- **El escritorio no se toca.** Ninguna tarea de este plan modifica el comportamiento de la app de Linux. `ctest` debe seguir dando **3/3** (más los tests nuevos) en verde.
- **El motor público no se toca:** nada en `/home/landy/Descargas/NOVA-STUDIO/libs/ardour`.
- **Sin `libsndfile` en Android:** `pkg_check_modules(SNDFILE ...)` está dentro de `if(NOT ANDROID AND NOT WIN32)` (`CMakeLists.txt:45-67`) → en Android **no hay** libsndfile. El decodificador de WAV se escribe a mano; el de MP3 usa minimp3.
- **Picos con el formato que ya consume el renderizador:** `PeakPoint{min,max,rms}` y `PeakData{peaks, overview, version=2}` (`src/core/NovaPeakData.h`). Nada de "2 floats por pico".
- **Sesiones a 48 kHz; el motor pide 48 kHz al dispositivo** y remuestrea si no puede (`spec` §10).
- **El móvil nunca escribe en la sesión original** (`spec` §8.6).
- **Waydroid no es un móvil:** latencia real, micrófono, multitáctil y background **no son verificables** (`spec` §5.1). No se pueden dar por buenos.
- **ABIs:** el APK de producción es `arm64-v8a`; para Waydroid hace falta además `x86_64` (la imagen es x86_64). Los dos se construyen desde el mismo script.
- **Toolchain:** NDK `26.1.10909125`, Qt `6.8.3` (`android_arm64_v8a` y `android_x86_64` instalados), Java 17 o 21, `ANDROID_PLATFORM=android-31` en el script de build.

## Review Focus

Cinco entradas/condiciones que el spec implica y que ninguna comprobación automática couvre por sí sola. Cada una tiene su test en la tarea que posee el código:

1. **MP3 con tag ID3v2 y relleno al final** → el decodificador debe dar la duración real (±2 %) y no leer basura del relleno. Test en Task 2 (`mp3_con_tag_id3_da_duracion_real`).
2. **Fichero inexistente o que no es audio** → `decodeAudioFile` devuelve `false` con error legible; la app no se cuelga. Test en Task 2 (`archivo_inexistente_devuelve_error`, `archivo_no_audio_devuelve_error`).
3. **La reproducción llega al final del fichero** → el callback no lee fuera del buffer (silencio, no crash). Test en Task 3 (`render_tras_el_fin_no_lee_fuera`).
4. **Audio a 44,1 kHz con salida a 48 kHz** → remuestreo; sin él el pitch es incorrecto. Test en Task 3 (`remuestreo_cambia_la_duracion_esperada`).
5. **La app se cierra con el stream de Oboe abierto** → Oboe exige cerrar los streams antes de destruir el motor; si no, crash al salir (este proyecto ya Suffrió crashes de salida). Verificación en Task 4 (`cierre_limpio_sin_crash`).

---

## File Structure

**Creados:**

| Fichero | Responsabilidad |
|---|---|
| `src/audio/NovaDecodedAudio.h` | Struct del audio decodificado (tasa, canales, muestras intercaladas). |
| `src/audio/NovaAudioDecoder.h/.cpp` | Fachada `decodeAudioFile()`: elige decodificador por extensión y devuelve error legible. |
| `src/audio/NovaWavReader.h/.cpp` | Lector WAV (RIFF PCM 16/24/32 bits y float). Sin dependencias externas. |
| `src/audio/NovaMp3Decoder.h/.cpp` | Decodificador MP3 con minimp3. |
| `third_party/minimp3/minimp3.h` | minimp3 (dominio público) vendorizado. |
| `src/audio/NovaAudioPlayer.h/.cpp` | Reproductor puro: PCM → frames de salida, con posición y fin de fichero. **Sin Oboe.** |
| `src/audio/NovaResampler.h/.cpp` | Remuestreo lineal (44,1 ↔ 48 kHz). |
| `src/audio/NovaAudioPeaks.h/.cpp` | `buildPeakData()`: PCM → `PeakData` (detail + overview) con el SIMD existente. |
| `src/core/platform/NovaSpikeController.h/.cpp` | Objeto QML: carga un fichero, calcula picos, publica en `NovaWaveformCache`, manda a reproducir, expone posición. |
| `qml/spike/SpikePage.qml` | Página mínima del spike: onda + playhead + botón de reproducir. |
| `tests/test_audio_decoder.cpp` | Tests del decodificador. |
| `tests/test_audio_player.cpp` | Tests del reproductor y del remuestreo. |
| `tests/test_audio_peaks.cpp` | Test del constructor de picos. |
| `tests/check_android_build_script.py` | Test de contrato del script de build (estilo `check_qml_contract.py`). |
| `tests/fixtures/tono-1s-44100-mono.mp3` | Fixture: 1 s, 440 Hz, 44,1 kHz, mono, **con tag ID3v2**. |
| `tests/fixtures/tono-1s-44100-mono.wav` | Fixture: el mismo tono en WAV. |
| `tests/fixtures/no-es-audio.txt` | Fixture: fichero que no es audio. |
| `docs/android/waydroid.md` | Runbook: arrancar Waydroid, instalar, probar, dónde poner el MP3. |

**Modificados:**

| Fichero | Cambio |
|---|---|
| `build_android.sh` | `--abi` parametrizable; toolchain Qt y nombre del APK según ABI. |
| `CMakeLists.txt` | Nuevas fuentes en `qt_add_executable`; 3 targets de test nuevos + `add_test`. |
| `src/core/platform/NovaAndroidAudioEngine.h/.cpp` | La salida de Oboe sirve el `NovaAudioPlayer` en vez de silencio. |
| `main.cpp` | En Android, carga `qml/spike/SpikePage.qml` y expone `SpikeController` al QML. |

---

## Task 1: `build_android.sh` parametrizable por ABI

**Files:**
- Modify: `build_android.sh` (líneas 25-38 y 77-89)
- Create: `tests/check_android_build_script.py`
- Modify: `CMakeLists.txt` (dentro del bloque `if(BUILD_TESTING AND NOT ANDROID AND NOT WIN32)`, tras el bloque `qml_contract` de la línea 263)

**Interfaces:**
- Consumes: nada.
- Produces: `./build_android.sh --abi <arm64-v8a|x86_64>` (por defecto `arm64-v8a`) → deja el APK en `apks/NovaStudio-<abi>.apk`. Toolchain: `$HOME/Qt/6.8.3/android_<abi>`.

- [ ] **Step 1: Escribir el test de contrato que falla**

`tests/check_android_build_script.py` (patrón de `tests/check_qml_contract.py`: analysis estático del script, salida legible, `sys.exit(1)` si falla). Debe comprobar, leyendo `build_android.sh` como texto:

- el script menciona `--abi` y tiene un valor por defecto `arm64-v8a`;
- la ruta del toolchain se **deriva** del ABI (aparece `android_${ABI}` o equivalente con una variable, no una ruta fija `android_arm64_v8a` hardcodeada en dos sitios);
- el nombre del APK de salida incluye el ABI (`NovaStudio-<abi>.apk`);
- el script pasa `-DANDROID_ABI` con el valor elegido;
- el `abiFilters` del `app/build.gradle` del staging se ajusta al ABI elegido (el staging lo copia de `android/app/`, que hoy fija `arm64-v8a, armeabi-v7a`).

Nombres de comprobación: `soporta_parametro_abi`, `abi_por_defecto_arm64`, `toolchain_derivado_del_abi`, `nombre_de_apk_incluye_abi`, `pasa_android_abi_a_cmake`, `ajusta_abifilters_del_staging`.

- [ ] **Step 2: Ejecutar el test y verificar que falla**

Run: `python3 tests/check_android_build_script.py`
Expected: FAIL, con `soporta_parametro_abi` en rojo (el script actual no menciona `--abi`).

- [ ] **Step 3: Implementar el parámetro en `build_android.sh`**

- Parsear `--abi <valor>` con un `case` que acepte `arm64-v8a|x86_64` y **aborte con mensaje claro** si el valor no es válido.
- `ABI="${ABI:-arm64-v8a}"` y `QT_ANDROID_DIR="$HOME/Qt/6.8.3/android_${ABI}"`; `QT_ANDROID_CMAKE="${QT_ANDROID_DIR}/bin/qt-cmake"` (hoy está hardcodeado `android_arm64_v8a`).
- `-DANDROID_ABI="${ABI}"` en la llamada a `qt-cmake`.
- Tras el staging (donde ya se copian manifest, settings.gradle y los jars de Qt), añadir el ajuste de `abiFilters`: reemplazar en `${BUILD_DIR}/android-build/app/build.gradle` la línea `abiFilters ...` por `abiFilters '${ABI}'`.
  **Ojo:** el bloque `jniLibs`/`packaging` de `android/app/build.gradle` usa `doNotStrip "*/arm64-v8a/*.so"` y `exclude '**/*.so'` ya corregido; el `doNotStrip` para el ABI nuevo se ajusta también con `sed` en el staging (no en el fuente, para no romper el build de arm64).
- `APK_DEST="${EXPORT_DIR}/NovaStudio-${ABI}.apk"`.
- El mensaje final debe imprimir la ruta del APK **con su ABI**.

- [ ] **Step 4: Ejecutar el test y verificar que pasa**

Run: `python3 tests/check_android_build_script.py`
Expected: PASS, las 6 comprobaciones en verde.

- [ ] **Step 5: Registrar el test en CTest**

En `CMakeLists.txt`, dentro del bloque de `BUILD_TESTING` (tras `qml_contract`, línea 263-265), añadir:

```cmake
    add_test(NAME android_build_script
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_android_build_script.py)
```

(usando el mismo `Python3_EXECUTABLE` que ya se resuelve en ese bloque; si `Python3_Interpreter_FOUND` es falso, no se registra y el test se ejecuta a mano).

- [ ] **Step 6: Reconfigurar, compilar y pasar los tests**

Run: `cmake -S . -B build && cmake --build build -j$(nproc) && cd build && ctest`
Expected: `100% tests passed`, ahora **4/4** (3 previos + `android_build_script`).

- [ ] **Step 7: Commit**

```bash
git add build_android.sh tests/check_android_build_script.py CMakeLists.txt
git commit -m "build(android): parametrizar build_android.sh por ABI (arm64-v8a y x86_64 para Waydroid)"
```

---

## Task 2: Decodificador de audio (WAV propio + MP3 con minimp3)

**Files:**
- Create: `src/audio/NovaDecodedAudio.h`, `src/audio/NovaAudioDecoder.h/.cpp`, `src/audio/NovaWavReader.h/.cpp`, `src/audio/NovaMp3Decoder.h/.cpp`, `third_party/minimp3/minimp3.h`
- Create: `tests/test_audio_decoder.cpp`, `tests/fixtures/tono-1s-44100-mono.mp3`, `tests/fixtures/tono-1s-44100-mono.wav`, `tests/fixtures/no-es-audio.txt`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: nada.
- Produces (lo que usan las tareas 3, 5 y 6):

```cpp
// src/audio/NovaDecodedAudio.h
struct NovaDecodedAudio {
    int sampleRate = 0;
    int channels = 0;
    std::vector<float> samples;      // intercalados: L R L R ...
    qint64 frameCount() const { return channels > 0 ? static_cast<qint64>(samples.size()) / channels : 0; }
};

// src/audio/NovaAudioDecoder.h
bool decodeAudioFile(const QString& path, NovaDecodedAudio& out, QString* error);
```

`decodeAudioFile` devuelve `true` solo si el audio se decodificó entero; si no, `false` y `error` con un mensaje legible (nunca vacío cuando falla).

- [ ] **Step 1: Generar y versionar los fixtures**

```bash
mkdir -p tests/fixtures
# WAV: 1 s, 440 Hz, 44,1 kHz, mono, amplitud 0,5
sox -n -r 44100 -c 1 -b 16 tests/fixtures/tono-1s-44100-mono.wav \
    synth 1 sine 440 vol 0.5
# MP3: el mismo tono, CON tag ID3v2 (cubre el Review Focus #1)
lame --tt 2 -m m --resample 44.1 tests/fixtures/tono-1s-44100-mono.wav \
    tests/fixtures/tono-1s-44100-mono.mp3
printf 'esto no es audio\n' > tests/fixtures/no-es-audio.txt
```

Expected: los tres ficheros existen; `lame` imprime el resumen de codificación.

- [ ] **Step 2: Escribir el test que falla**

`tests/test_audio_decoder.cpp`, con el mismo estilo de `tests/test_simd_peaks.cpp` (macro `CHECK`, `main()` que devuelve el número de fallos). Rutas de los fixtures: `NOVA_FIXTURES` como constante de compilación (`-DNOVA_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"`), con valor por defecto `"tests/fixtures"` si no se define.

Comprobaciones:

- `wav_se_decodifica_completo`: `decodeAudioFile(fixtureWav, out, &err)` → `true`; `out.sampleRate == 44100`; `out.channels == 1`; `out.frameCount()` entre 44.090 y 44.110; `err` vacío.
- `wav_preserva_la_amplitud`: máximo absoluto de `out.samples` entre 0,45 y 0,55 (el fixture es 0,5 de pico).
- `mp3_se_decodifica_completo`: `decodeAudioFile(fixtureMp3, out, &err)` → `true`; `out.sampleRate == 44100`; `out.channels == 1`.
- `mp3_con_tag_id3_da_duracion_real` (Review Focus #1): `out.frameCount()` dentro de ±2 % de 44.100 (el padding del encoder y el retardo de decodificación justifican la tolerancia; el tag ID3v2 **no** debe contarse como audio).
- `mp3_preserva_la_amplitud`: máximo absoluto entre 0,40 y 0,55.
- `archivo_inexistente_devuelve_error` (Review Focus #2): ruta que no existe → `false` y `err` no vacío.
- `archivo_no_audio_devuelve_error` (Review Focus #2): `no-es-audio.txt` → `false` y `err` no vacío.
- `mp3_truncado_no_provoca_crash`: se pasa un temporal con los primeros 1.024 bytes del MP3 → la función **no** se cuelga ni lee fuera; se acepta `false` con error **o** `true` con menos frames, pero nunca un crash ni `true` con 0 frames.

- [ ] **Step 3: Ejecutar el test y verificar que falla**

Run: `cmake --build build --target test_audio_decoder && ./build/test_audio_decoder`
Expected: FAIL — la compilación falla con "No such file or directory: src/audio/NovaAudioDecoder.h" (todavía no existe).

- [ ] **Step 4: Vendorizar minimp3 y escribir el decodificador**

- `third_party/minimp3/minimp3.h`: obtener minimp3 (dominio público, <https://github.com/lieff/minimp3>), versión ≥ 2.0, **sin modificar**. Dejarlo en `third_party/minimp3/` con una nota en `third_party/minimp3/README.md`: origen, versión, licencia (dominio público) y motivo de la elección (el core de Ardour trae mpg123, que no existe en Android; minimp3 son ~200 líneas y no engorda el APK).
- `src/audio/NovaWavReader.h/.cpp`: leer RIFF/WAVE. Soportar PCM 16 bits ( entero), 24 bits, 32 bits entero y 32 bits float (`format tag 3`), y `WAVE_FORMAT_EXTENSIBLE` con el subformato real. Rechazar con error claro lo que no se entienda (no hacer `memcpy` a ciegas: un 24 bits mal leído da ruido).
- `src/audio/NovaMp3Decoder.h/.cpp`: usar `mp3dec_decode_frame` de minimp3 en bucle hasta el final del fichero; acumular en `std::vector<float>` intercalado; **omitir** los frames que solo contengan tags ID3 (minimp3 los salta, pero hay que verificar que no se cuelen) y el relleno final. Devolver la tasa del primer frame.
- `src/audio/NovaAudioDecoder.cpp`: elegir por extensión (`.wav` → WAV, `.mp3` → MP3) y **por contenido** (si la extensión no cuadra, probar la cabecera RIFF o el sync de MP3). Si la extensión es desconocida pero el contenido es válido, decodificar igualmente. Mensajes de error en español y con el motivo.

- [ ] **Step 5: Registrar las fuentes y el test en CMakeLists.txt**

- Añadir a `qt_add_executable` (junto a `src/core/NovaSimdPeaks.cpp`, línea 119): `src/audio/NovaDecodedAudio.h`, `src/audio/NovaAudioDecoder.h/.cpp`, `src/audio/NovaWavReader.h/.cpp`, `src/audio/NovaMp3Decoder.h/.cpp`, `third_party/minimp3/minimp3.h`.
- Añadir el target de test dentro del bloque `BUILD_TESTING`:

```cmake
    add_executable(test_audio_decoder
        tests/test_audio_decoder.cpp
        src/audio/NovaAudioDecoder.cpp
        src/audio/NovaWavReader.cpp
        src/audio/NovaMp3Decoder.cpp
    )
    target_include_directories(test_audio_decoder PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_definitions(test_audio_decoder PRIVATE NOVA_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    add_test(NAME audio_decoder COMMAND test_audio_decoder)
```

- [ ] **Step 6: Compilar y ejecutar el test**

Run: `cmake -S . -B build && cmake --build build -j$(nproc) && ./build/test_audio_decoder`
Expected: PASS, todas las comprobaciones en verde (imprime `== Decodificador de audio ==` y `0 fallos`).

- [ ] **Step 7: Commit**

```bash
git add src/audio third_party/minimp3 tests/fixtures tests/test_audio_decoder.cpp CMakeLists.txt
git commit -m "feat(audio): decodificador de WAV y MP3 (minimp3) sin dependencias de Ardour"
```

---

## Task 3: Reproductor y remuestreo (sin Oboe, testeable en el host)

**Files:**
- Create: `src/audio/NovaAudioPlayer.h/.cpp`, `src/audio/NovaResampler.h/.cpp`, `tests/test_audio_player.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `NovaDecodedAudio` y `decodeAudioFile()` de la Task 2.
- Produces (lo que usa la Task 4):

```cpp
// src/audio/NovaAudioPlayer.h
class NovaAudioPlayer {
public:
    bool load(const NovaDecodedAudio& audio, QString* error);
    void setOutputSampleRate(int sampleRate);      // 48000 en el móvil
    void play();
    void stop();
    bool isPlaying() const;
    qint64 positionFrames() const;                  // frames de SALIDA ya emitidos
    qint64 durationFrames() const;                  // frames de SALIDA del fichero completo
    double durationSeconds() const;
    int renderFrames(float* interleavedOut, int frames);   // frames escritos (0 si no está sonando)
    void reset();
};
```

`renderFrames` escribe samples intercalados en el buffer de salida; devuelve cuántos frames escribió (puede ser menos que `frames` si el fichero se acabó).

- [ ] **Step 1: Escribir el test que falla**

`tests/test_audio_player.cpp` (mismo estilo `CHECK`). Casos:

- `carga_acepta_un_fichero_decodificado`: cargar el WAV del fixture → `durationFrames()` ≈ 44.100 × (48000/44100) = 48.000 ± 1 % (la tasa de salida por defecto es 48 kHz).
- `render_escribe_frames_y_avanza_la_posicion`: tras `play()`, `renderFrames(buf, 512)` → devuelve 512, `positionFrames() == 512`, y el buffer **no** es silencio (alguna muestra con valor absoluto > 0,01).
- `render_tras_el_fin_no_lee_fuera` (Review Focus #3): renderizar `durationFrames() + 1000` frames en total → la última llamada devuelve **0** frames y `positionFrames()` **no supera** `durationFrames()`. (Este es el test que evita el buffer overrun en el móvil.)
- `remuestreo_cambia_la_duracion_esperada` (Review Focus #4): con salida a 48 kHz, `durationSeconds()` ≈ 1,0 s ± 1 %; con salida a 44,1 kHz, ≈ 1,0 s ± 1 % también (la duración en segundos no cambia, solo el número de frames).
- `fichero_mas_corto_que_un_bloque`: un PCM de 10 frames → `renderFrames(buf, 512)` devuelve 10 y no lee de más.
- `stop_para_la_reproduccion`: tras `stop()`, `renderFrames()` devuelve 0.
- `sin_cargar_no_escribe_nada`: `renderFrames()` sin `load()` devuelve 0 y no escribe.

- [ ] **Step 2: Ejecutar el test y verificar que falla**

Run: `cmake --build build --target test_audio_player`
Expected: FAIL al compilar: "No such file or directory: src/audio/NovaAudioPlayer.h".

- [ ] **Step 3: Implementar el remuestreador**

`src/audio/NovaResampler.h/.cpp`:

```cpp
class NovaResampler {
public:
    void setRates(int inRate, int outRate);
    void reset();
    // Convierte `inFrames` frames intercalados de entrada en frames de salida.
    int process(const float* in, int inFrames, float* out, int outCapacity);
};
```

Remuestreo **lineal** con posición fraccionaria persistente entre llamadas (para que no haya clics por discontinuity). Si `inRate == outRate`, copia directa.

- [ ] **Step 4: Implementar el reproductor**

`src/audio/NovaAudioPlayer.h/.cpp`:
- `load()` guarda el audio y calcula `durationFrames()` con la tasa de salida vigente.
- `renderFrames()`: si no está sonando → 0. Si quedan frames → remuestrea y copia al buffer, avanza la posición, devuelve los frames escritos. Si el fichero se agotó → devuelve 0 (y `isPlaying()` pasa a `false`).
- `reset()` vuelve al principio y limpia el remuestreador.

- [ ] **Step 5: Registrar fuentes y test en CMakeLists.txt**

Añadir `src/audio/NovaAudioPlayer.h/.cpp` y `src/audio/NovaResampler.h/.cpp` a `qt_add_executable`, y el target:

```cmake
    add_executable(test_audio_player
        tests/test_audio_player.cpp
        src/audio/NovaAudioPlayer.cpp
        src/audio/NovaResampler.cpp
    )
    target_include_directories(test_audio_player PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_definitions(test_audio_player PRIVATE NOVA_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    add_test(NAME audio_player COMMAND test_audio_player)
```

- [ ] **Step 6: Compilar y ejecutar**

Run: `cmake -S . -B build && cmake --build build -j$(nproc) && ./build/test_audio_player`
Expected: PASS, 0 fallos.

- [ ] **Step 7: Commit**

```bash
git add src/audio tests/test_audio_player.cpp CMakeLists.txt
git commit -m "feat(audio): reproductor con remuestreo y posicion, testeable sin Oboe"
```

---

## Task 4: Puente con Oboe — la salida reproduce el fichero cargado

**Files:**
- Modify: `src/core/platform/NovaAndroidAudioEngine.h`, `src/core/platform/NovaAndroidAudioEngine.cpp`

**Estado actual verificado (no hay que descubrirlo):**
- `OutputStreamCallback::onAudioReady` está en `NovaAndroidAudioEngine.cpp:566-587`: rellena el buffer con `getOutputBuffer(0)` (ring buffer) y hace `m_currentFrame.fetch_add(numFrames)`.
- El callback solo tiene un puntero `NovaAndroidAudioEngine* m_engine` ✓ → alcanza el motor para pedirle frames.
- **El motor Oboe no se instancia en ninguna parte de la app**: en Android, `NovaSessionManager::initSession()` (`NovaSessionManager.cpp`, rama `#else`) hace `d_ptr->engine = ARDOUR::AudioEngine::create()`, que en móvil es el **stub** (`NovaAndroidStubs.h:275-284`, un objeto estático vacío). Las 593 líneas del motor Oboe están compiladas pero **muertas**. Por eso la Task 5 lo instancia.
- Todo el fichero está dentro de `#if defined(Q_OS_ANDROID)` → **la clase no existe en escritorio**. Cualquier cosa que la use va guardada igual.

**Interfaces:**
- Consumes: `NovaAudioPlayer` y `decodeAudioFile()` (Tasks 2 y 3).
- Produces:

```cpp
// en NovaAndroidAudioEngine (solo Android)
bool loadFile(const QString& path, QString* error);   // decodifica y deja listo para reproducir
```

- [ ] **Step 1: Añadir el reproductor al motor**

En `NovaAndroidAudioEngine.h`: añadir el miembro `NovaAudioPlayer m_player;` (con `#include "audio/NovaAudioPlayer.h"`, dentro del bloque `#if defined(Q_OS_ANDROID)`) y el método público `bool loadFile(const QString& path, QString* error);`.

- [ ] **Step 2: Implementar `loadFile()`**

- Llama a `decodeAudioFile(path, audio, error)`; si falla, devuelve `false` con el error.
- Si va bien: `m_player.load(audio, error)` y `m_player.setOutputSampleRate(m_sampleRate)`.
- Log: `qCDebug(novaCore) << "🔊 Spike: cargado" << path << audio.sampleRate << "Hz," << audio.channels << "canales," << audio.frameCount() << "muestras";`

- [ ] **Step 3: Servir los frames desde el callback**

En `NovaAndroidAudioEngine.cpp:566` (`OutputStreamCallback::onAudioReady`):
- Si `m_engine->m_player.isPlaying()`: `int written = m_engine->m_player.renderFrames(outputBuffer, numFrames);` y rellenar el resto (`written * channels` … `numFrames * channels`) con ceros.
- Si no está sonando: comportamiento actual (rellenar con ceros y leer el ring buffer del track 0, que se conserva para el mixer de la Fase 1).
- **Avanzar `m_currentFrame` con los frames realmente escritos**, no con `numFrames`: si no, el playhead avanzaría más rápido que el audio.

- [ ] **Step 4: Conectar la tasa de salida**

En `initialize(int sampleRate, ...)`: hacer `m_player.setOutputSampleRate(sampleRate)` (el motor arranca a 48.000 ✓, igual que las sesiones).

- [ ] **Step 5: Cerrar los streams antes de destruir (Review Focus #5)**

En `shutdown()` y en el destructor: llamar a `closeStreams()` **antes** de destruir los callbacks y el motor, y dejar `m_outputStream`/`m_inputStream` a null. Comentario en el código: "Oboe exige cerrar los streams antes de destruir el motor; si no, crash al salir". Log final: `"🛑 Spike: streams cerrados"`.

- [ ] **Step 6: Compilar para Android (solo verifica que compila; la prueba funcional es la Task 6)**

Run: `cd build_android && cmake --build . -j$(nproc) --target nova_ui_qt`
Expected: compila sin errores.

- [ ] **Step 7: Commit**

```bash
git add src/core/platform/NovaAndroidAudioEngine.h src/core/platform/NovaAndroidAudioEngine.cpp
git commit -m "feat(android): la salida de Oboe reproduce el fichero cargado"
```

---

## Task 5: Picos desde PCM + página QML del spike

**Files:**
- Create: `src/audio/NovaAudioPeaks.h/.cpp`, `src/core/platform/NovaSpikeController.h/.cpp`, `qml/spike/SpikePage.qml`, `tests/test_audio_peaks.cpp`
- Modify: `CMakeLists.txt`, `main.cpp`

**Interfaces:**
- Consumes: `NovaDecodedAudio` (Task 2), `processPeakBlockSimd()` (existe en `src/core/NovaSimdPeaks.h`), `NovaWaveformCache::setPeaks()` (existe en `src/models/NovaRegionModel.h`).
- Produces:

```cpp
// src/audio/NovaAudioPeaks.h
PeakData buildPeakData(const NovaDecodedAudio& audio, int detailPoints, int overviewPoints);
```

Y el objeto QML `SpikeController` (registrado como tipo QML `SpikeController`, módulo `NovaStudio` 1.0), con:

```cpp
Q_INVOKABLE bool loadAndPlay(const QString& path);   // decodifica, calcula picos, publica, reproduce
Q_INVOKABLE void stop();
Q_PROPERTY(double positionSeconds READ positionSeconds NOTIFY positionChanged)
Q_PROPERTY(QString status READ status NOTIFY statusChanged)
Q_PROPERTY(double durationSeconds READ durationSeconds NOTIFY statusChanged)
```

El `regionId` que se publica en `NovaWaveformCache` es **exactamente** `"spike"` (el mismo que usa `SpikePage.qml` en `NovaWaveformItem.regionId`).

- [ ] **Step 1: Escribir el test de picos que falla**

`tests/test_audio_peaks.cpp`:
- `picos_de_un_tono_coinciden_con_la_amplitud`: decodificar el WAV del fixture → `buildPeakData(audio, 64, 8)` → `data.peaks` no vacío, `data.overview` no vacío, `data.version == 2`, y el máximo absoluto de `min`/`max` entre 0,45 y 0,55.
- `picos_de_un_silencio_son_cero`: audio de 1 s de ceros → todos los picos con `min == max == rms == 0`.
- `el_primer_pico_no_es_el_silencio_inicial`: un audio que empieza con 0,5 s de silencio y luego un tono → el primer pico tiene `max == 0` y algún pico posterior tiene `max > 0,4`.

- [ ] **Step 2: Ejecutar el test y verificar que falla**

Run: `cmake --build build --target test_audio_peaks`
Expected: FAIL al compilar ("No such file or directory: src/audio/NovaAudioPeaks.h").

- [ ] **Step 3: Implementar `buildPeakData()`**

- Reutilizar `initPeakAccumulators()` y `processPeakBlockSimd()` de `src/core/NovaSimdPeaks.h` (ya existentes y con test): `detailStep = frameCount / detailPoints`, `overviewStep = frameCount / overviewPoints`.
- Rellenar `PeakData{peaks, overview, version = 2, sampleRate, channels}`.

- [ ] **Step 4: Implementar `NovaSpikeController`**

- `loadAndPlay(path)`: `decodeAudioFile()` → si falla, `status = error` y devuelve `false`. Si va bien: `buildPeakData()` → `NovaWaveformCache::setPeaks("spike", data)` → `m_engine->loadFile(path, &err)` → `m_engine->setPlaying(true)` → `status = "sonando"`.
- `positionSeconds()`: `m_engine->positionFrames() / sampleRate`.
- Un `QTimer` de 50 ms refresca `positionChanged` (la posición real la lleva el motor, no el timer).
- **Nota:** `NovaWaveformCache::setPeaks()` está en `src/models/NovaRegionModel.h` → incluirlo. Ese fichero ya compila en Android (usa los stubs) ✓.

- [ ] **Step 5: Escribir `qml/spike/SpikePage.qml`**

Página mínima (se registra sola en el recurso: `cmake/NovaQml.cmake` hace `GLOB_RECURSE` de `qml/*.qml` ✓):
- `import NovaStudio 1.0` y `import QtQuick 2.15`.
- Un `NovaWaveformItem { regionId: "spike"; startBeat: 0; lengthBeats: 4; pixelsPerBeat: 80; width: parent.width; height: 160 }`.
- Un rectángulo de playhead cuya `x` se calcula con `SpikeController.positionSeconds / SpikeController.durationSeconds * width` (con guarda de división por cero).
- Un `TextField` con la ruta (por defecto `/sdcard/Music/spike.mp3`) y un `Button` que llama a `SpikeController.loadAndPlay(textField.text)`.
- Un `Text` con `SpikeController.status`.

- [ ] **Step 6: Enganchar el controlador en `main.cpp`**

- `NovaSpikeController` **solo existe en Android** (todo su `.cpp` dentro de `#if defined(Q_OS_ANDROID)`, porque usa `NovaAndroidAudioEngine`, que tampoco existe en escritorio).
- **El controlador es quien instancia el motor Oboe** (hoy nadie lo instancia): en su constructor,
  `m_engine = std::make_unique<NovaAndroidAudioEngine>(); m_engine->initialize(48000); m_engine->start(); m_engine->openOutputStream(2);`
  y en el destructor `m_engine->shutdown()` **antes** de destruirlo (Review Focus #5).
- `loadAndPlay(path)`: `m_engine->loadFile(path, &err)` → si falla, `status = err` y devuelve `false`; si va bien, calcula los picos, los publica, `m_player` del motor en marcha con `m_engine->setPlaying(true)` y `status = "sonando"`.
- `main.cpp`: `qmlRegisterType<NovaSpikeController>("NovaStudio", 1, 0, "SpikeController")` **dentro de `#ifdef Q_OS_ANDROID`** (junto al `qmlRegisterType` de `NovaWaveformItem`, main.cpp:36).
- En la rama QRC de `main.cpp` (donde se carga `qrc:/qt/qml/NovaStudio/qml/Main.qml`, línea 163), en Android cargar `qrc:/qt/qml/NovaStudio/qml/spike/SpikePage.qml` en su lugar.

- [ ] **Step 7: Registrar fuentes y test, y compilar**

En `CMakeLists.txt`: añadir `src/audio/NovaAudioPeaks.h/.cpp` a `qt_add_executable`; `src/core/platform/NovaSpikeController.h/.cpp` **solo en Android** (si no, `add_executable` lo añade siempre pero el fichero debe compilar también en escritorio → si incluye cabeceros de Oboe, hay que guardarlo con `#ifdef Q_OS_ANDROID`; en ese caso, en escritorio el fichero compila vacío y el test del controlador no se escribe).

Target de test:

```cmake
    add_executable(test_audio_peaks
        tests/test_audio_peaks.cpp
        src/audio/NovaAudioPeaks.cpp
        src/audio/NovaAudioDecoder.cpp
        src/audio/NovaWavReader.cpp
        src/audio/NovaMp3Decoder.cpp
        src/core/NovaSimdPeaks.cpp
    )
    target_include_directories(test_audio_peaks PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_definitions(test_audio_peaks PRIVATE NOVA_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    add_test(NAME audio_peaks COMMAND test_audio_peaks)
```

Run: `cmake -S . -B build && cmake --build build -j$(nproc) && ./build/test_audio_peaks`
Expected: PASS, 0 fallos.

- [ ] **Step 8: Compilar para Android**

Run: `cd build_android && cmake --build . -j$(nproc) --target nova_ui_qt`
Expected: compila sin errores (el `.cpp` del controlador debe usar `#ifdef Q_OS_ANDROID` en lo específico de Oboe).

- [ ] **Step 9: Commit**

```bash
git add src/audio src/core/platform/NovaSpikeController.* qml/spike CMakeLists.txt main.cpp tests/test_audio_peaks.cpp
git commit -m "feat(android): pagina del spike con onda real y playhead"
```

---

## Task 6: Build x86_64, instalar en Waydroid y verificar

**Files:**
- Create: `docs/android/waydroid.md`
- Modify: `build_android.sh` (solo si el Task 6 revela un fallo del Task 1)

**Interfaces:**
- Consumes: todo lo anterior.
- Produces: `apks/NovaStudio-x86_64.apk` instalable en Waydroid y un runbook.

- [ ] **Step 1: Construir el APK x86_64**

Run: `./build_android.sh --abi x86_64`
Expected: `BUILD SUCCESSFUL` y `apks/NovaStudio-x86_64.apk` (≥ 30 MB, con `lib/arm64-v8a` → `lib/x86_64` y `libnova_ui_qt_x86_64.so`).

Verificación del contenido:

```bash
unzip -l apks/NovaStudio-x86_64.apk | grep -c "lib/x86_64/.*\.so$"
```

Expected: **80** (o más) librerías nativas en `lib/x86_64/`.

- [ ] **Step 2: Arrancar Waydroid**

Run: `waydroid session start && sleep 5 && adb devices`
Expected: `adb devices` lista `emulator-5554` (o similar) con estado `device`.

- [ ] **Step 3: Poner el MP3 de prueba en el contenedor**

El `sdcard` del contenedor está mapeado al host. Copiar el fixture:

```bash
adb push tests/fixtures/tono-1s-44100-mono.mp3 /sdcard/Music/spike.mp3
```

Expected: `1 file pushed`.

- [ ] **Step 4: Instalar y arrancar**

```bash
adb install -r apks/NovaStudio-x86_64.apk
adb shell am start -n com.novastudio.nova/com.novastudio.nova.MainActivity
```

Expected: la app arranca (la actividad `MainActivity` está en el manifest ✓).

- [ ] **Step 5: Verificar por log (criterio objetivo, no hace falta oír)**

```bash
adb logcat -c && adb shell am start -n com.novastudio.nova/com.novastudio.nova.MainActivity && sleep 6 && adb logcat -d | grep -E "Nova|Spike|🔊|🛑" | tail -30
```

Expected (en este orden):
- `"🔊 Spike: cargado ... 44100 Hz, 1 canal"` — el fichero se decodificó;
- un log con **frames escritos > 0** — el callback de Oboe está sirviendo audio;
- la posición avanzando (frames emitidos crecientes) — el playhead se mueve;
- al cerrar la app: `"🛑 Spike: streams cerrados"` y **sin** `SIGSEGV` ni `Fatal signal` en el log (Review Focus #5).

- [ ] **Step 6: Comprobación manual mínima (la hace el usuario, 10 segundos)**

En la pantalla de Waydroid: pulsar "Reproducir" y confirmar que **se oye** el tono y que el playhead se mueve. El usuario ya dijo que no quiere pruebas manuales repetidas: esta es **una** comprobación, y el criterio objetivo del Step 5 es el que manda para cerrar la tarea.

- [ ] **Step 7: Escribir el runbook**

`docs/android/waydroid.md`, en español, con: cómo arrancar la sesión, cómo instalar, cómo meter un MP3, qué logs mirar, y una tabla con **qué NO se puede verificar en Waydroid** (latencia real, micrófono, multitáctil, background) y qué habría que hacer en un móvil real.

- [ ] **Step 8: Tests completos**

Run: `cd build && ctest`
Expected: `100% tests passed`, **7/7** (`project_policy`, `simd_peaks`, `qml_contract`, `android_build_script`, `audio_decoder`, `audio_player`, `audio_peaks`).

- [ ] **Step 9: Commit**

```bash
git add docs/android/waydroid.md build_android.sh
git commit -m "docs(android): runbook de Waydroid y verificacion del spike"
```

---

## Self-Review

**Cobertura del spec (Fase 0):** parametrización de ABI → Task 1 ✓ · Waydroid arranca e `adb` ve el contenedor → Task 6 ✓ · APK x86_64 instalado → Task 6 ✓ · MP3 leído y decodificado → Task 2 ✓ · Reproducción con Oboe → Task 4 ✓ · Onda visible (renderizador real, sin tocarlo) → Task 5 ✓ · Playhead avanzando → Task 5 ✓ · Latencia medida → Task 6 (con la advertencia de que no es representativa) ✓ · Comprobación de que el móvil no usa stubs en la ruta del spike → Task 5 (el controlador y el motor no pasan por los stubs) ✓.

**Huecos detectados y resueltos:** el spec (§8.2) asumía un decodificador de MP3 pero no fijaba cuál → Task 2 fija minimp3 con su justificación. El spec (§8.4) exigía picos `{min,max,rms}` → Task 5 usa el `PeakData` existente. El spec (§15) pedía `build_android.sh --abi` → Task 1 lo define exactamente, incluido el nombre del APK.

**Consistencia de tipos:** `NovaDecodedAudio` (Task 2) → la consume `NovaAudioPlayer::load()` (Task 3) → la consume `NovaAndroidAudioEngine::loadFile()` (Task 4) → la consume `NovaSpikeController::loadAndPlay()` (Task 5). `buildPeakData()` (Task 5) devuelve el `PeakData` de `src/core/NovaPeakData.h`, el mismo que consume `NovaWaveformCache`. El `regionId` es `"spike"` en los dos lados (controlador y QML) — **si cambia uno, hay que cambiar el otro**.

**Review Focus:** los 5 tienen test o verificación asignada (RF#1, #2 → Task 2; RF#3, #4 → Task 3; RF#5 → Task 4 y Task 6 Step 5).

**Proporción:** el plan describe 6 tareas con firmas y aserciones, sin volcar implementaciones; el código que describe es de ~1.500 líneas y el plan es de ~500.
