# NOVA Studio móvil — diseño del DAW Android

- **Fecha:** 2026-10-20
- **Estado:** borrador para revisión del constructor (builder check). Sin código escrito todavía.
- **Autor:** OpenCode (MiniMax M3.1 Flash) junto con el usuario.
- **Repos:**
  - `nova_ui_qt` — la app Qt6/QML (privado). **Aquí vive todo el trabajo de este documento.**
  - `NOVA-STUDIO` — fork público de Ardour (el motor de escritorio). **No se toca** salvo que se indique explícitamente.
- **Idioma del trabajo:** español (código, comentarios y documentación).

---

## 1. Intención (el porqué)

NOVA Studio quiere ser **superior a BandLab en móvil**. La palabra "superior" es ambigua y este documento la fija:

BandLab es un producto de **nube**: el proyecto vive en sus servidores, se paga por suscripción para lo bueno (mastering, efectos Pro), y su edición es *track-based* (grabar, Repetir, mezclar, publicar). NOVA no puede competir ahí: es un producto de un solo desarrollador contra el presupuesto, la comunidad y el marketplace de ellos.

Donde NOVA **sí** puede ganar de forma defendible es en el eje que BandLab **estructuralmente no puede ofrecer**:

> **NOVA móvil = el DAW que abre la sesión que tenías en el escritorio y te deja seguir editándola en el teléfono, sin cuenta, sin nube y sin suscripción.**

Ese eje es defendible porque el proyecto de NOVA en escritorio **ya es un formato real y portable** (una carpeta de sesión de Ardour), mientras que el de BandLab está atado a su nube. Además, la edición real por clip (cortar, dividir, fades, ganancia, automatización) y el mixer de verdad son cosas que un DAW móvil pequeño puede hacer bien y BandLab no hace.

**El objetivo final no es "tener app en Android": es que en un móvil se pueda abrir un proyecto de escritorio, tocarlo, editarlo a mano y exportar un WAV, sin crear una cuenta.**

---

## 2. Objetivos

1. **El móvil reproduce y edita proyectos de escritorio.** Abrir una carpeta de sesión de Ardour en Android, ver el timeline con sus clips, oírlos, moverlos, cortarlos, exportar.
2. **Motor propio en móvil**, escrito desde cero, sin depender del core de Ardour (que no compila en Android).
3. **El escritorio no se rompe.** Ardour sigue siendo el motor de escritorio; el comportamiento actual no cambia; los tests siguen en verde.
4. **Sin cuenta, sin nube, sin paywall** para lo básico. Los proyectos son carpetas en el disco del usuario.
5. **Una sola base de código de interfaz.** El QML, los modelos y el renderizador de onda se comparten; solo cambia el motor detrás.

## 3. No-objetivos (no vamos a hacer esto)

- No se porta el core de Ardour a Android. No se escribe un backend Oboe para `ARDOUR::AudioEngine`.
- No se implementan plugins de terceros en móvil (VST3/AU/AAX): en Android no existe ecosistema y no se puede crear.
- No se compite en red social, comunidad, marketplace, publicación a Spotify ni mastering con IA.
- No se migra el escritorio al motor nuevo: el escritorio sigue con Ardour durante todo el proyecto.
- No se toca el motor público `NOVA-STUDIO` (Ardour) salvo que aparezca un bloqueo demostrable, y con permiso explícito del usuario.
- No se borran las skins muertas (`bandlab`, `reaper`, `draft`, ~3.700 líneas) sin permiso. **La skin `logic` es la única en uso.**

## 4. Anti-objetivos (lo que sería un fracaso)

- **Un APK que arranca y no hace nada.** El APK actual compila y empaqueta, pero llama a clases vacías (stubs) y por eso no hace nada. Eso es peor que no tener APK: es una promesa rota.
- **Un mini-Ardour a medias.** Si el motor móvil crece sin que aparezca edición real usable, se está construyendo un juguete. Hay un criterio de corte (§13).
- **Romper el escritorio.** Cualquier regresión en el DAW de escritorio para poder enseñar algo en el móvil es un fracaso, aunque el móvil quede bien.
- **Un móvil atado a la GPL.** El motor de Ardour es GPL v2+ (`libs/ardour/COPYING`). El motor móvil se escribe **limpio**, sin código de Ardour, para que el móvil no dependa de esa licencia. *(El estado de licencia del escritorio es un asunto ya existente y fuera del alcance de este documento; lo debe revisar el asesor legal del usuario, no este diseño.)*

## 5. Restricciones

### 5.1 Límites de Waydroid (el entorno de pruebas)

Waydroid es un Android en un contenedor: sirve para validar **lógica, decodificación, interfaz y formato**, pero **no** reproduce el hardware de un móvil. Estos límites son conocidos y hay que tenerlos presentes al cerrar fases:

| Qué | En Waydroid | Consecuencia |
|---|---|---|
| **ABI** | La imagen es **x86_64**; nuestro APK actual es **arm64-v8a** → **no arranca**. Hay que construir además la variante **x86_64** (`Qt/6.8.3/android_x86_64` ✓ instalado, NDK ✓). | `build_android.sh` debe parametrizar el ABI. Entregable de la fase 0. |
| **Audio de salida** | HAL virtual sobre el audio del host (PipeWire/Pulse, 48 kHz). Oboe **no encuentra AAudio** y cae a OpenSL ES / HAL emulado. | **La latencia medida en Waydroid no es representativa.** El camino de baja latencia (AAudio) **no se puede verificar aquí**. |
| **Latencia real** | La del host (varios ms, con jitter). | El objetivo de latencia móvil queda **sin verificar** hasta que haya un móvil real. Se documenta como riesgo abierto (§14). |
| **Entrada táctil** | El ratón emula el táctil. | Se validan **toque y arrastre**. Los **gestos multitáctiles (pinza de zoom) NO se pueden validar** → la fase 4 queda parcialmente bloqueada. |
| **Micrófono** | El host puede exponer su entrada; depende de la configuración de Waydroid. | La **grabación** puede quedar sin verificar. |
| **Doze / background** | No hay gestión de energía real. | El `AudioForegroundService` y el `WAKE_LOCK` se implementan **siguiendo buenas prácticas, sin poder verificarlos aquí**. |
| **Almacenamiento** | El `sdcard` del contenedor es la carpeta del host → el móvil puede ver (y escribir) ficheros del PC. | **Muy útil para probar**: la sesión del escritorio es directamente visible desde Waydroid. |

**Consecuencia de planificación:** las fases 0-3 se pueden cerrar en Waydroid. La fase 4 (interfaz táctil) se cierra **a medias**, y el trabajo de campo (grabación, background, latencia) queda pendiente de un móvil real. Antes de publicar a usuarios habrá que hacer una **revisión en hardware** (§13).

### 5.2 Restricciones generales

| Restricción | Detalle |
|---|---|
| **Tiempo** | Un desarrollador. ~3 meses a tiempo completo hasta la v1 creíble. |
| **Entorno de pruebas** | **Waydroid**, no un móvil físico. Imagen `waydroid_x86_64` instalada (`/var/lib/waydroid/images/system.img`), host x86_64, audio del host por PipeWire/Pulse a **48 kHz float32** ✓ (coincide con las sesiones). Límites reales de este entorno (§5.1). |
| **Toolchain Android** | NDK 26.1.10909125, Qt 6.8.3 `android_arm64_v8a`, Java 17 o 21, SDK 35, Gradle 8.10, AGP 8.2.2. Todo ya instalado y funcionando (`./build_android.sh`). |
| **Toolchain escritorio** | Ardour 8.x compilado en el repo padre (`build/libs/ardour`), JACK vía `pw-jack`. |
| **Sin plugins de terceros** | Restricción dura (§3). |
| **Latencia móvil** | Con AAudio/Oboe, round-trip típico de 10-30 ms. Suficiente para grabar voz/instrumentos; insuficiente para competir con un DAW de escritorio en instrumentación a tiempo real. |
| **Doze / background** | Android mata procesos en background: la grabación necesita `AudioForegroundService` (ya declarado en el manifest) + `WAKE_LOCK`. |

## 6. Estado actual verificado (medido, no supuesto)

Medido sobre el código hoy:

- **Acoplamiento con Ardour:** solo **8 ficheros** incluyen headers de Ardour. Dos concentran casi todo:
  - `src/models/NovaRegionModel.cpp` → **52** referencias a `ARDOUR::`
  - `src/models/NovaTrackListModel.cpp` → **32** referencias
  - resto: `NovaSessionManager` (23), `NovaTransportController` (16), `NovaProjectManager` (11), `NovaAudioEngine` (8), `NovaRecordManager` (4), `NovaAudioDeviceManager` (4).
- **Stubs de móvil:** `src/core/platform/NovaAndroidStubs.h` = 291 líneas de clases ARDOUR falsas. `Source::read()` devuelve 0 muestras; `Session` no guarda pistas reales; `import_files()` no hace nada. **Por eso el móvil "no hace nada": no es un fallo, es un mueble vacío.**
- **Motor Oboe existente:** `src/core/platform/NovaAndroidAudioEngine.cpp` = 593 líneas, **ya tiene** ring buffer, streams de entrada/salida, `diskWriterLoop()` y escritura de WAV. **No tiene reproducción de archivos** (ni decodificador ni lectura). Es la base del motor nuevo, no hay que escribirlo desde cero.
- **Decodificadores:** la app enlaza `sndfile` (CMakeLists:65). `mpg123` viene **dentro del core de Ardour** → **no está disponible en Android**. Para MP3 en móvil hace falta un decodificador propio.
- **QML sin ramas Android:** `grep Q_OS_ANDROID qml/` → **0 resultados**. La UI es la misma en escritorio y móvil (hoy, layout de escritorio, no táctil).
- **94 bloques `#if Q_OS_ANDROID`** repartidos en 12 ficheros de `src/` → condicionales dispersos que hay que ir cerrando.
- **APK:** `apks/NovaStudio-arm64-v8a.apk`, 34 MB, 80 librerías nativas, manifest `com.novastudio.nova`. Compila y empaqueta (arreglado en el commit `363ed47`).
- **Windows también va contra los stubs:** `CMakeLists.txt:45` → `if(NOT ANDROID AND NOT WIN32)` para enlazar Ardour. O sea: **el build de Windows (`build_windows.sh` → `exes/NovaStudio-Win64.exe`, 13,6 MB) NO usa Ardour**, igual que Android. Windows es, hoy, un tercer "caparazón" sin motor. **Decidido: fuera de alcance** (§16.1 decisión 3).
- **Tests:** 3 tests en `ctest`, todos en verde.

---

## 7. Arquitectura

### 7.1 El patrón

El problema actual: los modelos llaman **directamente** a `ARDOUR::Session`. En Android, como Ardour no existe, alguien escribió clases falsas que **imitan** la API de Ardour. Resultado: la app móvil compila, pero cada llamada va a un mueble vacío, y además la lógica de importación de escritorio no se puede reutilizar en móvil (rompe la compilación, como pasó con el import rápido).

La solución: **una interfaz propia** (`nova::NovaEngine`) que define qué necesita la app de un motor de audio. Dos implementaciones: la de escritorio (envuelve Ardour) y la de móvil (motor propio con Oboe).

```
        QML (skin logic) + modelos Qt + renderizador de onda      ← se reutilizan tal cual
                                    │
                                    ▼
                    ┌───────────────────────────────┐
                    │      nova::NovaEngine          │  ← interfaz propia (nueva)
                    │  (src/core/NovaEngine.h)       │
                    └───────────────┬───────────────┘
             ┌──────────────────────┴──────────────────────┐
             ▼                                             ▼
    ┌──────────────────┐                        ┌────────────────────┐
    │  ArdourEngine    │  escritorio            │   AndroidEngine    │  móvil
    │ (src/core/plat/  │                        │ (src/core/platform/│
    │  ArdourEngine.*) │                        │  AndroidEngine.*)  │
    │  ≈ 400-600 lín.  │                        │  ≈ 1.500-2.000 lín.│
    └──────────────────┘                        └────────────────────┘
      envuelve ARDOUR::Session                    Oboe + mixer + decoder
```

**Por qué esta forma y no "rellenar los stubs":** los stubs obligan a que el API falso crezca hasta imitar todo Ardour (y a que la app siga hablando un idioma que no es el suyo). La interfaz propia hace que **el código de la app sea el mismo en las dos plataformas** y que cada motor hable su idioma.

### 7.2 La interfaz `nova::NovaEngine` (esbozo de API)

Es un esbozo: el constructor ajusta firmas y tipos. Lo que no se negocia es **el reparto de responsabilidades**: todo lo que hoy depende de Ardour en los modelos pasa a esta interfaz.

```cpp
// src/core/NovaEngine.h
namespace nova {

// Una fuente de audio: un canal de una región.
// En Ardour una región estéreo son DOS fuentes mono (no un archivo estéreo),
// y así se modela aquí también (§9).
struct NovaSource {
    QString path;           // ruta absoluta al archivo de audio
    int     channel;        // 0 = izquierdo, 1 = derecho, -1 = mono
    qint64  offsetSamples;  // offset dentro del archivo
    qint64  lengthSamples;  // duración
    float   gain;           // ganancia lineal (1.0 = unity)
};

struct NovaRegion {
    QString id;
    QString name;
    qint64  positionSamples;
    qint64  lengthSamples;
    float   gainDb = 0.0f;      // ganancia de la región
    float   fadeInBeats = 0.0f; // fades expressed in beats
    float   fadeOutBeats = 0.0f;
    QVector<NovaSource> sources; // 1 (mono) o 2 (estéreo)
};

struct NovaTrack {
    QString id;
    QString name;
    float   gain  = 1.0f;     // fader
    float   pan   = 0.0f;     // -1..1
    bool    mute  = false;
    bool    solo  = false;
    bool    recordEnable = false;
    QVector<NovaRegion> regions;   // ordenadas por posición
};

struct NovaTransportState {
    bool   playing   = false;
    bool   recording = false;
    qint64 positionSamples = 0;
    qint64 loopStart = -1;
    qint64 loopEnd   = -1;
    double tempo = 120.0;
    int    beatsPerBar = 4;
};

// Picos para la onda. El formato debe ser el que ya consume el renderizador:
// un pico = {min, max, rms} (el renderizador pinta min/max y la rms aparte).
struct NovaPeak {
    float min = 0.0f;
    float max = 0.0f;
    float rms = 0.0f;
};
struct NovaPeakBuffer {
    QString sourcePath;
    qint64  offsetSamples;
    qint64  lengthSamples;
    QVector<NovaPeak> peaks;
};

class NovaEngine : public QObject {
    Q_OBJECT
public:
    virtual ~NovaEngine() = default;

    // ── ciclo de vida ─────────────────────────────────────────────
    virtual bool start(QString* error) = 0;
    virtual void stop() = 0;

    // ── proyecto ──────────────────────────────────────────────────
    virtual bool openProject(const QString& path, QString* error) = 0;
    virtual bool saveProject(QString* error) = 0;
    virtual QString projectPath() const = 0;

    // ── pistas ─────────────────────────────────────────────────────
    virtual QVector<NovaTrack> tracks() const = 0;
    virtual bool addAudioTrack(QString* error) = 0;
    virtual bool removeTrack(int index, QString* error) = 0;
    virtual void setTrackGain(int index, float gain) = 0;
    virtual void setTrackPan(int index, float pan) = 0;
    virtual void setTrackMute(int index, bool mute) = 0;
    virtual void setTrackSolo(int index, bool solo) = 0;
    virtual void setTrackRecordEnable(int index, bool enabled) = 0;

    // ── edición de regiones ───────────────────────────────────────
    // (La skin logic llama hoy a TODAS estas: moveRegion, resizeRegion,
    //  removeRegion, splitRegion, setClipGainDb, setFadeInBeats,
    //  setFadeOutBeats, normalizeClip. Si la interfaz no las cubre,
    //  el escritorio deja de editar → §3/§4 lo prohíben.)
    virtual bool moveRegion(int trackIndex, const QString& regionId,
                            qint64 newPosition, QString* error) = 0;
    virtual bool resizeRegion(int trackIndex, const QString& regionId,
                              qint64 newLength, QString* error) = 0;
    virtual bool removeRegion(int trackIndex, const QString& regionId,
                              QString* error) = 0;
    virtual bool splitRegion(int trackIndex, const QString& regionId,
                             qint64 atSamples, QString* error) = 0;
    virtual bool setRegionGainDb(int trackIndex, const QString& regionId,
                                 float gainDb, QString* error) = 0;
    virtual bool setRegionFades(int trackIndex, const QString& regionId,
                                float fadeInBeats, float fadeOutBeats,
                                QString* error) = 0;
    virtual bool normalizeRegion(int trackIndex, const QString& regionId,
                                 QString* error) = 0;

    // ── transporte ─────────────────────────────────────────────────
    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void locate(qint64 samples) = 0;
    virtual NovaTransportState transport() const = 0;

    // ── importación y picos ────────────────────────────────────────
    // Importa sin re-codificar: copia el archivo y crea la fuente sobre la copia.
    virtual bool importAudio(const QString& filePath, int trackIndex,
                             qint64 atSamples, QString* error) = 0;
    virtual void requestPeaks(const QString& sourcePath, qint64 offsetSamples,
                              qint64 lengthSamples, int wantedPeaks,
                              quint64 requestId) = 0;

signals:
    void transportChanged();
    void projectChanged();
    void metersUpdated(const QVector<float>& meters);
    void peaksReady(quint64 requestId, const nova::NovaPeakBuffer& peaks);
    void errorRaised(const QString& message);
};
}
```

**Nota sobre el alcance de la interfaz:** la lista de arriba **no es opcional ni minimalista**: recoge todo lo que la skin `logic` (la única en uso) llama hoy a los modelos. Si `ArdourEngine` no implementa una de esas, el escritorio pierde esa función y el proyecto viola sus propios anti-objetivos (§4).

**Regla de oro:** los modelos (`NovaRegionModel`, `NovaTrackListModel`) **no saben nada** del motor que hay debajo. Donde hoy hay `m_session->...` o `ARDOUR::...`, mañana hay `m_engine->...`. La skin QML tampoco cambia.

### 7.3 Reparto por fichero

| Fichero | Acción | Notas |
|---|---|---|
| `src/core/NovaEngine.h` / `.cpp` | **NUEVO** | Interfaz + estructuras + un `createEngine()` que devuelve la implementación correcta según plataforma. |
| `src/core/platform/ArdourEngine.h` / `.cpp` | **NUEVO** | Adaptador sobre `ARDOUR::Session*`. Traduce `NovaTrack`↔`ARDOUR::Route`, `NovaRegion`↔`ARDOUR::Region`. **≈ 900-1.400 líneas**: no es un envoltorio trivial, porque debe reimplementar sobre Ardour el import sin recodificar, el split real de región, los fades, el normalize y la lectura de picos con cambio de tasa, que hoy viven en `NovaRegionModel` y en `NovaTrackListModel`. |
| `src/core/platform/AndroidEngine.h` / `.cpp` | **NUEVO** | Motor propio con Oboe. Reutiliza el ring buffer y el grabador de `NovaAndroidAudioEngine`. |
| `src/models/NovaRegionModel.cpp` | **MODIFICAR** | Quitar el uso directo de Ardour (52 refs). Import y picos pasan por el motor. |
| `src/models/NovaTrackListModel.cpp` | **MODIFICAR** | Quitar el uso directo de Ardour (32 refs). M/S/REC leen del motor. |
| `src/core/NovaSessionManager.cpp` | **MODIFICAR** | Crea el motor en vez de la sesión de Ardour en móvil; en escritorio crea el `ArdourEngine`. |
| `src/core/NovaTransportController.cpp` | **MODIFICAR** | Habla con el motor, no con `ARDOUR::Transport`. |
| `src/core/platform/NovaAndroidStubs.h` | **REEMPLAZAR** | Los stubs son la implementación de mentira; se sustituyen por el motor real. El fichero desaparece (o queda solo lo estrictamente no-ARDOUR). |
| `src/core/platform/NovaAndroidAudioEngine.cpp` | **MODIFICAR** | Se queda como **base** del motor: ring buffer + streams + grabación. Se le añade reproducción. |
| `src/core/NovaProjectManager.cpp` | **MODIFICAR** | En móvil, abrir proyecto = leer sesión de Ardour (§9). **Añadir**: importador de proyectos NOVA (`.novaproject` / `project.json`) → crea una sesión Ardour nueva en el escritorio (fase 3, §16.2 decisión 1). |
| `src/core/NovaRecordManager.cpp` | **MODIFICAR** | Idem con la grabación. |
| QML (`qml/skins/logic/**`) | **MODIFICAR (fase 4)** | Layout táctil. El modelo de datos de QML no cambia. |

### 7.4 Qué cambia en el escritorio (y qué no)

- **No cambia el comportamiento.** La skin Logic hace lo mismo.
- Los modelos dejan de incluir headers de Ardour y hablan con `NovaEngine`.
- `ArdourEngine` traduce. El trabajo por debajo (import rápido, picos, M/S/REC, cámara) sigue igual, solo cambia **dónde** se llama.
- Los 3 tests de `ctest` deben seguir en verde **en todo momento**. Si un cambio los rompe, ese cambio va mal.

---

## 8. Motor Android (`AndroidEngine`)

### 8.1 Arquitectura de audio

- **Oboe** (ya integrado vía `FetchContent` en `CMakeLists.txt:16-20`) con preferencia por **AAudio** y `PerformanceMode::LowLatency` en el stream de salida.
- Flujo: **archivo → decodificador → buffer de pista → mezclador → ring buffer → callback de Oboe → dispositivo**.
- El callback **nunca** debe bloquear ni decodificar: solo suma buffers ya preparados.
- **Reloj de transporte:** un `std::atomic<qint64>` con la posición en muestras, avanzado por el callback según los frames escritos. Es la fuente de verdad del playhead (no un temporizador del hilo GUI).

### 8.2 Decodificación

- **WAV/FLAC/AIFF:** `libsndfile` (ya es dependencia de la app). Hay que compilarlo para Android en el build (`build_android.sh`); verificar que la toolchain NDK lo soporta.
- **MP3:** `mpg123` no está disponible en móvil (viene con Ardour). Opciones, por orden de preferencia:
  1. **minimp3** (dominio público, ~200 líneas, decodificador MP3 entero) → compilarlo dentro del proyecto.
  2. `mpg123` compilado para Android (más trabajo).
  3. `ffmpeg` (pesado, ~10 MB más de APK).
- **Decisión pendiente de fase 1**, con la restricción: el APK no debería crecer más de ~1 MB por el decodificador.
- **PCM/WAV propio** para la grabación: el grabador existente (`writeWavFile`) ya sabe escribir WAV.

### 8.3 Mezcla y transporte

- Mixer por pista: `fuente → ganancia (fader) → pan (equal-power) → mute/solo → bus/master`.
- Barato y suficiente: no hace falta un bus de efectos en la v1.
- `solo` se resuelve globalmente: si alguna pista tiene solo activo, las que no lo tienen se silencian.
- Transporte: play/pause/stop, `locate(samples)`, loop opcional (fase 2), posición en muestras → beats con el tempo de la sesión para la UI.

### 8.4 Picos (waveform)

- La lógica de picos **ya existe y funciona** en escritorio (`NovaRegionModel::buildRegionPeaks` + `requestRegionPeaks`, con conversión de tasa de muestreo). Se **extrae** a un servicio común que usa el motor.
- Se calcula **en un hilo aparte**, nunca en el hilo GUI.
- **Formato de pico: `{min, max, rms}`** (tres floats), que es lo que ya consume el renderizador (`NovaWaveformRenderer` pinta min/max y la rms con su propio color). Si `requestPeaks` devolviera "2 floats por pico", habría que tocar el renderizador → no se toca.
- El motor móvil puede **reutilizar los picos precalculados** de la carpeta `peaks/` de la sesión (formato NPK2 de Ardour) cuando existan, y calcularlos solo si faltan.
- El renderizador (`NovaWaveformRenderer`) ya tiene variante de shader para OpenGL ES 3 ✓ → no hay que tocarlo.

### 8.5 Grabación

- El grabador existente (`diskWriterLoop`, `writeWavFile`) se reutiliza y se adapta al modelo de pistas nuevo.
- Graba a WAV en la carpeta del proyecto; el archivo se referencia como una fuente más.
- `REC` por pista, `MONITOR` y `input` de latencia: fuera del alcance de la v1 salvo lo mínimo.

### 8.6 Persistencia (decidido)

- **Fase 1-2: el proyecto es una sesión de Ardour**, y el móvil la abre **en modo lectura**. El móvil **no escribe sesiones de Ardour** (nunca).
- **Cómo llega al móvil (decidido):** el usuario elige la carpeta de sesión con el selector de Android (`ACTION_OPEN_DOCUMENT_TREE`) → la app toma el permiso persistente → se hace una **copia de trabajo** de la sesión en el almacenamiento privado de la app (`filesDir/projects/<nombre>/`). El motor trabaja sobre esa copia con **rutas absolutas** (§9.1) y **nunca toca la sesión original**.
  - Por qué copia y no "in situ": el motor necesita rutas reales (§10), y el acceso "en sitio" por SAF son URIs. La copia lo resuelve sin permisos especiales y funciona en cualquier Android 8+ y en Waydroid.
  - Si el usuario concede `MANAGE_EXTERNAL_STORAGE`, se puede abrir **in situ** (sin copia) — mejora opcional para sesiones gigantes, no bloqueante.
- **Formato propio NOVA** (fase 2, cuando haya edición que guardar): una carpeta con `project.json` + los archivos de audio.
  - `project.json`: versión, pistas (nombre, fader, pan, M/S), regiones (posición, largo, offset, ganancia, fades), fuentes (ruta relativa), tempo, compás, loop.
  - Rutas **relativas** dentro de la carpeta del proyecto → la carpeta es movible y exportable.
- **Exportar proyecto NOVA (`.novaproject`)**: un ZIP con `project.json` + audio, que se escribe en el `sdcard` (visible desde el PC en Waydroid, y exportable por SAF en un móvil). Es el vehículo de **vuelta al escritorio** (§12, fase 3).
- **El escritorio sigue leyendo sesiones de Ardour** y, además, entendendrá proyectos NOVA mediante un importador (§7.3).

---

## 9. Leer sesiones de Ardour en el móvil

### 9.1 Formato (verificado contra sesiones reales del usuario)

Verificado leyendo el código de Ardour **y** las sesiones reales en `~/Documentos/NovaProjects/` (`Proyecto 2/3/4/5`, `DefaultSession`):

- **Fichero de sesión:** `<carpeta-de-sesión>/<nombre>.ardour` — XML, `version="7003"` (Ardour 9.8). **No** es `.session`.
- **Directorios dentro de la sesión** (`libs/ardour/directory_names.cc`):
  - `interchange/<nombre-de-sesión>/audiofiles/` → **el audio importado**. OJO: el subdirectorio lleva el **nombre de la sesión**, no es `interchange/audiofiles/` a secas.
  - `interchange/<nombre-de-sesión>/midifiles/`
  - `peaks/` → picos precalculados (¡reutilizables en móvil!)
  - `export/`, `backup/`, `routestates/`, `plugin_metadata/`, `surfaces/`
- **El XML NO guarda rutas**: cada `<Source>` guarda solo el **basename** (`name="audio%L.wav"`), y se resuelve con el *search path* de la sesión. La región enlaza con sus fuentes por `source-0`/`source-1` + `channels`; la playlist enlaza con la ruta vía `orig-track-id`.
- **Una región estéreo = DOS fuentes mono** (archivos `%L`/`%R` sueltos, 48 kHz / 32-bit float en las sesiones del usuario). El modelo de datos del motor tiene que soportarlo (§7.2: `NovaRegion::sources`).
- **El XML no guarda rutas absolutas**, así que el parser tiene que reconstruir la búsqueda: basename → `interchange/<sesión>/audiofiles/`, luego `peaks/`, luego la carpeta de fuentes configurada en la sesión, y como último recurso el **nombre de archivo** (para audios externos que el usuario movió de sitio).

**Consecuencia de diseño:** el motor trabaja con **rutas absolutas** (no URIs). Eso condiciona el modelo de acceso a archivos en Android (§10).

### 9.2 Alcance de la lectura (v1)

- Pistas de audio: nombre, fader, pan, M/S.
- Playlists: clips (posición, largo, offset dentro del archivo, ganancia).
- Resolución de rutas: relativas a la sesión; si no existen, buscar por nombre en `interchange/audiofiles/` y en la carpeta de "sources".
- **Se ignoran** (con aviso en log, no con fallo): plugins, automatizaciones, MIDI, buses con envíos, matrices de ganancia, Lua, superficies.

### 9.3 Tarea de investigación obligatoria (principio de fase 1)

El formato exacto de Ardour 8/9 cambia entre versiones. Antes de escribir el parser, **hay que derivarlo del código**:
- `libs/ardour/session.cc` (lectura/escritura del XML de sesión)
- `libs/ardour/region.cc` (nodo `<Region>` y sus propiedades)
- `libs/ardour/playlist.cc` (estructura de los clips en la playlist)
- `libs/ardour/session_directory.cc` (rutas de `interchange/`, `peaks/`, `audiofiles/`)
- `libs/ardour/session_file_data.cc` / `libs/ardour/region_list.cc` (datos auxiliares)

Y **validarlo contra una sesión real** (hay proyectos del usuario en `~/Documentos/NovaProjects/` y sesiones de prueba que se pueden generar con Ardour). El parser se prueba con: sesión con audio importado, sesión con audio externo (fuera de la carpeta), sesión con pistas MIDI (debe ignorarlas sin fallar).

---

## 10. Android: lo específico de la plataforma

| Tema | Decisión |
|---|---|
| **Almacenamiento / acceso a archivos (decidido)** | **SAF (`ACTION_OPEN_DOCUMENT_TREE`) + copia de trabajo en el almacenamiento privado de la app** (§8.6). El motor trabaja con **rutas absolutas** (§9.1), así que se resuelve la ruta copiando, no con URIs. Ventajas: sin permisos especiales, funciona en Android 8+ y en Waydroid, y publicable en Play. `MANAGE_EXTERNAL_STORAGE` queda como **mejora opcional** (abrir in situ sin copiar). Consecuencia: **el móvil nunca escribe en la sesión original**. El manifest declara hoy `MANAGE_EXTERNAL_STORAGE`; se mantiene como opcional y se añade el flujo SAF. |
| **Tasa de muestreo / remuestreo** | Las sesiones del usuario son de **48 kHz**; muchos móviles tienen 44,1 kHz nativo. Decisión: el motor **pide 48 kHz** a Oboe y, si el dispositivo no lo da, **remuestrea en el motor** (lineal o cúbica). Sin esto, el móvil suena afinado una octava por debajo en la mitad de los teléfonos. |
| **Permisos** | `RECORD_AUDIO`, `MODIFY_AUDIO_SETTINGS`, `WAKE_LOCK`, `FOREGROUND_SERVICE` + `FOREGROUND_SERVICE_MICROPHONE` (ya en el manifest). |
| **Copia de trabajo** | La app copia la carpeta de sesión a `filesDir/projects/<nombre>/` y trabaja ahí. La copia es la **única** que se modifica. Los picos calculados se guardan en la copia (no en la original). |
| **Servicio en primer plano** | `AudioForegroundService` ya existe en `android/app/src/main/java/com/novastudio/nova/AudioForegroundService.java`; hay que declarar `foregroundServiceType="microphone|mediaPlayback"` en el manifest y que el servicio sobreviva al "Doze". |
| **Doze / background** | Con la pantalla apagada, Android puede pausar el proceso. `WAKE_LOCK` + servicio en primer plano + `setKeepScreenOn` en la actividad. |
| **Audio route** | Oboe debe reaccionar a los cambios de ruta (auriculares Bluetooth, USB). A probar: enchufar/desenchuchar el jack → cambia el número de frames. |
| **Latencia** | `LowLatency` + `AAudio`; si el dispositivo no lo soporta, fallback a `OpenSL ES` (Oboe lo hace solo). |
| **JNI** | El motor corre en C++ dentro del proceso; la interfaz Qt (`NovaEngine`) lo consume **directamente en C++**, sin JNI. JNI solo hace falta para lo que toque la API de Android (permisos, servicio, rutas). |

---

## 11. Interfaz táctil

El QML **no tiene ramas Android** y el layout actual es de escritorio (panel izquierdo, timeline horizontal, mixer como panel). En un móvil eso no funciona. Decisiones:

- **Reutilizar la skin `logic`**, no escribir una skin nueva: los componentes (TCP, timeline, clips, mixer) ya hacen lo correcto; lo que falta es **composición** (vertical, por pestañas) y **tamaño de toque** (mínimo 44 px) y **gestos** (arrastrar clip, pinza para zoom).
- **Fase 4**, no antes: hasta la fase 3, se usa el layout de escritorio en horizontal (en un móvil en horizontal puede servir para probar el motor).
- Lo que hay que construir: un `MobileWorkspace.qml` (o variantes del `WorkspaceRouter.qml`) que componga: barra de transporte arriba → timeline → pestañas (Pistas / Mixer / Efectos).
- **La skin `bandlab` del repo NO sirve como base móvil**: es una skin de escritorio (rack/TCP/topbar, 1.731 líneas). No confundir "bandlab" (skin) con "BandLab" (producto).
- **En Waydroid el táctil es el ratón**: se validan toque y arrastre, pero **la pinza de zoom no se puede probar** (§5.1). Los gestos multitáctiles quedan pendientes de hardware real.

---

## 12. Fases

Cada fase tiene un entregable verificable. **Una fase no está cerrada hasta que su criterio de "hecho" se cumple en el entorno de pruebas (Waydroid).**

### Fase 0 — Spike: que suene algo en el móvil

**Objetivo:** validar, en el entorno de pruebas (Waydroid), el camino de riesgo máximo (decodificación + Oboe + UI).

**Tareas:**
1. **Parametrizar `build_android.sh` por ABI** (`--abi arm64-v8a | x86_64`): hoy está fijo en arm64-v8a y **la imagen de Waydroid es x86_64**, así que hoy el APK ni arranca ahí. Toolchain `Qt/6.8.3/android_x86_64` ya instalado ✓.
2. Arrancar Waydroid (`waydroid session start`) y comprobar que `adb devices` ve el contenedor.
3. Instalar el APK x86_64 (`adb install -r ...`) y ver que arranca.
4. Leer un MP3 de una ruta conocida (`/sdcard/Music/...` o de la carpeta del host montada en el contenedor) y decodificarlo con el decodificador elegido.
5. Reproducirlo con el stream de Oboe existente (`NovaAndroidAudioEngine`), sin mixer: un archivo, directo a salida.
6. Mostrar la forma de onda (reutilizando el renderizador actual) y un playhead que avanza.
7. Medir la latencia **en Waydroid**, sabiendo que **no es representativa** (§5.1).

**Hecho cuando:** en Waydroid suena un MP3 con onda visible y el playhead avanza a la velocidad correcta.
**Riesgo que elimina:** decodificador + Oboe + empaquetado + ABI. Si esto falla, todo lo demás es detener y replantear.
**Lo que NO se puede cerrar aquí:** latencia real, micrófono, gestos multitáctiles (§5.1).

### Fase 1 — Motor móvil + interfaz `NovaEngine`

**Objetivo:** la app móvil deja de usar los stubs y usa un motor real; el escritorio no se toca.

**Tareas:**
1. `NovaEngine` + estructuras (interfaz pura, sin motor).
2. `ArdourEngine`: adaptador sobre `ARDOUR::Session*`, cubriendo **todo** el surface que la skin llama hoy (§7.2).
3. Migrar `NovaRegionModel` y `NovaTrackListModel` para hablar con `NovaEngine` (**los 3 tests de `ctest` en verde en todo momento**; el escritorio debe sonar y verse igual).
4. `AndroidEngine` v0: reproduce N archivos en paralelo (sin mixer real todavía, o con mixer simple), transporte con posición.
5. **Abrir proyecto en el móvil**: selector de carpeta (SAF) → copia de trabajo → parser de sesión de Ardour (§9.3). En Waydroid, la carpeta del host es directamente visible en el `sdcard` del contenedor ✓.
6. El APK **deja de usar `NovaAndroidStubs.h`** (los stubs quedan solo para Windows, §16.2 decisión 3).

**Hecho cuando:** en Waydroid se abre una sesión de Ardour real (del `~/Documentos/NovaProjects/` del usuario), se ve el timeline con sus clips y su onda, y suena.
**Hecho (escritorio):** `ctest` 3/3 y el escritorio sin cambios observables.

### Fase 2 — DAW móvil v1: mixer y edición

**Objetivo:** editar de verdad.

**Tareas:**
1. Mixer: fader, pan, M/S por pista + buses + metering.
2. Edición de regiones: mover, dividir, ganancia, fades, normalize.
3. Guardado propio: `project.json` + audio (formato NOVA, §8.6).
4. **Exportar proyecto NOVA** (`.novaproject`, ZIP con `project.json` + audio) al `sdcard` → vehículo de vuelta al escritorio.
5. Loop y metrónomo.

**Hecho cuando:** en el móvil se edita una región, se guarda, se reabre y se vuelve a oír igual; y el `.novaproject` exportado contiene lo esperado.

### Fase 3 — La apuesta: continuidad de ida y vuelta

**Objetivo:** el diferenciador.

**Tareas:**
1. Abrir una sesión de Ardour del escritorio en el móvil, editarla y exportarla (el parser y la edición ya existen de las fases 1-2; aquí se une todo y se exporta a WAV).
2. **Importador de proyectos NOVA en el escritorio**: abrir un `.novaproject` en NOVA de escritorio → crea una sesión Ardour nueva con sus pistas, regiones y audio (usando el import rápido ya existente). Esto es lo que hace la continuidad **de ida y vuelta** (§16.2 decisión 1).
3. Exportar WAV (mezcla y/o stems) desde el móvil.

**Hecho cuando:** una sesión creada en el escritorio → abierta y editada en el móvil → exportada como `.novaproject` → **abierta en el escritorio** con el mismo contenido.

### Fase 4 — Interfaz táctil y pulido

**Objetivo:** que se pueda usar con el dedo.

**Tareas:**
1. `MobileWorkspace.qml`: composición vertical por pestañas.
2. Gestos: arrastrar región, pinza de zoom, barrido del playhead.
3. Tamaños de toque, áreas seguras (notch), rotación.
4. Grabación con el servicio en primer plano, siguiendo buenas prácticas (§5.1: **no verificable en Waydroid**).

**Hecho cuando:** una sesión se edita entera con el dedo (táctil emulado por ratón en Waydroid) y la parte multitáctil queda **pendiente de revisión en un móvil real** (§13).

### Fase 5 — Decidir con datos

Revisar qué se ha construido contra los objetivos: ¿sirve? ¿qué falta para que un usuario real lo use? Decidir el siguiente eje (más edición, MIDI, efectos, o parar).

---

## 13. Kill criteria (cuándo replantear)

Se aplica **en cada fase**:

| Señal | Acción |
|---|---|
| El motor móvil supera **~3.000 líneas** y sigue sin edición real usable | Parar y replantear el alcance del móvil |
| **6 meses** sin una versión jugable en un móvil | Replantear el proyecto móvil entero |
| Aparece un requisito de **plugins de terceros en móvil** | Descartar la idea (no hay ecosistema; ni la opción (a) lo arreglaba) |
| El objetivo real resulta ser **paridad total con el escritorio** | Solo la vía (a) (portar Ardour) lo da, y está descartada → replantear el producto |
| No hay **entorno de pruebas** disponible (Waydroid no arranca, o falta la imagen) | Las fases 0-3 no avanzan; el trabajo se limita a escritorio |
| La fase 4 (táctil) se cierra solo a medias con el ratón de Waydroid | Antes de publicar a usuarios hace falta una **revisión en un móvil real** (gestos, micrófono, background, latencia). No publicar sin ella |

---

## 14. Riesgos

| Riesgo | Impacto | Mitigación |
|---|---|---|
| **Acoplamiento oculto en los modelos** | La migración a `NovaEngine` se come más de lo previsto | Los 8 ficheros están medidos; migración incremental con `ctest` en verde en cada paso |
| **Rendimiento en móvil** | La mezcla por pista puede no llegar en un gama media | Medir en el spike (fase 0) con 4-8 pistas; si falla, menos Simultaneidad o mezclar en `float`SIMD |
| **Formato de sesiónvaries entre versiones de Ardour** | El parser falla con sesiones nuevas | Investigación previa (§9.3) + probar con sesiones reales de varias versiones; si el formato es inestable, el parser solo soporta un subconjunto documentado |
| **Doze y background** | La grabación se corta al bloquear el móvil | `AudioForegroundService` + `WAKE_LOCK`; probar explícitamente con la pantalla apagada |
| **Interfaz táctil descuidada** | El motor funciona pero la app no se puede usar | Fase 4 dedicada; no dar por buena la v1 sin probar con el dedo (Waydroid emula el ratón; los gestos multitáctiles quedan pendientes de hardware real) |
| **Waydroid no representa a un móvil** | Se da por bueno algo que en un móvil real no funciona (latencia, multitáctil, background, micrófono) | §5.1 lista los límites; los riesgos no verificables quedan marcados como tales y se revisan antes de publicar |
| **Windows se queda como caparazón** | El `.exe` arranca pero no hace nada de DAW | Fuera de alcance explícito (§16.2 decisión 3); no se invierte en mantenerlo |
| **El escritorio se rompe** | Se pierde lo que ya funciona | Regla dura: `ctest` en verde siempre; cambios de escritorio solo con motivo |
| **Licencias** | El móvil hereda GPL por descuido | El motor móvil se escribe limpio, sin código de Ardour. Revisión legal aparte |

---

## 15. Verificación

```bash
# ── escritorio ────────────────────────────────────────────────
cd ~/Descargas/NOVA-STUDIO/nova_ui_qt
cmake --build build -j$(nproc)          # compilar
cd build && ctest                      # 3/3 obligatorios

# ── Android ────────────────────────────────────────────────────
./build_android.sh --abi arm64-v8a       # APK para móvil real
./build_android.sh --abi x86_64          # APK para Waydroid (imagen x86_64)
ls -la apks/*.apk                       # debe pesar >30 MB y traer libnova_ui_qt

# ── entorno de pruebas: Waydroid ───────────────────────────────
waydroid session start
adb devices                             # debe listar el contenedor
adb install -r apks/NovaStudio-x86_64.apk
adb logcat | grep -i nova                # logs de la app
```

**Regla de verificación:** ninguna fase se da por cerrada sin ejecutarla **en el entorno de pruebas**. Compilar no es funcionar. Y lo que Waydroid no puede verificar (latencia real, micrófono, multitáctil, background) queda **marcado como pendiente de hardware** (§5.1), no como hecho.

---

## 16. Decisiones

### 16.1 Decisiones tomadas (el usuario delegó: "lo que sea mejor")

1. ** Ida y vuelta: SÍ.** El escritorio podrá abrir los proyectos NOVA que edite el móvil → hay que añadir un importador `project.json` → sesión Ardour en la app de escritorio (fase 3, §7.3 y §12).
   - Por qué: la promesa central ("tus proyectos no viven en la nube de nadie") solo vale si es bidireccional. Sin vuelta, lo editado en el móvil sale como WAV y se pierde la trazabilidad.
2. **Acceso a archivos: SAF + copia de trabajo** en el almacenamiento privado de la app (§8.6, §10). `MANAGE_EXTERNAL_STORAGE` queda como mejora opcional (abrir in situ). El móvil **nunca escribe en la sesión original**.
   - Por qué: el motor necesita rutas absolutas; SAF + copia evita reescribir la capa de IO de audio, funciona sin permisos especiales, en Android 8+ y en Waydroid, y es publicable en Play.
3. **Windows: fuera de alcance, explícitamente.** No se invierte en un motor WASAPI ahora. Los stubs quedan **solo para Windows** (`NovaAndroidStubs.h` → guard `Q_OS_WIN` solamente cuando el motor móvil aterrice). Si más adelante se quiere Windows, es la **tercera** implementación de `NovaEngine` con el mismo patrón.
   - Por qué: dispersar el trabajo entre dos plataformas sin motor cuesta más que lo que aporta ahora; el `.exe` sigue compilando y arrancando como hoy.
4. **Entorno de pruebas: Waydroid** (no hay móvil físico). Implica: construir también la variante **x86_64** del APK, y aceptar que latencia real, micrófono, multitáctil y background **no son verificables** (§5.1). Antes de publicar a usuarios, revisión en hardware real (§13).

### 16.2 Decisiones del usuario

- El objetivo es **superior a BandLab** (no "tener app en Android").
- Se acepta que el móvil use un **motor propio** y que el escritorio **no se toque**.
- La skin en uso es **`logic`**; las demás se conservan pero no se tocan.
- Se acepta la definición del eje: **continuidad con el escritorio + edición real + sin cuenta/nube**.
- No hay móvil físico: las pruebas se hacen en **Waydroid**.

### 16.3 Decisiones mías (se pueden corregir en la revisión)

1. Interfaz `nova::NovaEngine` como costura; `ArdourEngine` y `AndroidEngine` como implementaciones.
2. El móvil **no escribe** sesiones de Ardour: usa un formato propio (`project.json`) solo cuando haya edición que guardar.
3. Fases en el orden 0→1→2→3→4, con la continuidad (fase 3) antes que el formato propio (dentro de la fase 2).
4. Decodificador MP3: minimp3 (dominio público) antes que mpg123 o ffmpeg.
5. Sin plugins en móvil: es definitivo, no "más adelante".
6. La interfaz táctil es fase 4, no fase 0 (primero el motor).
7. La interfaz `NovaEngine` debe cubrir **todo** lo que la skin `logic` llama hoy a los modelos (§7.2); si no, el escritorio pierde funciones y el proyecto viola sus anti-objetivos.
8. Los picos se expresan como `{min, max, rms}` para no tocar el renderizador (§8.4).
9. El motor pide 48 kHz al dispositivo y remuestrea si no puede (§10).
10. Las **reglas de sesión verificadas** (§9.1) mandan sobre cualquier suposición: `.ardour` (no `.session`), `interchange/<sesión>/audiofiles/`, basenames sin ruta, región estéreo = 2 fuentes mono.

---

## 17. Fuera de alcance

- Cualquier cambio en el motor público `NOVA-STUDIO` (Ardour).
- **Windows**: motor propio (WASAPI). El `.exe` sigue compilando y arrancando como hoy, pero **sin motor** (§16.1 decisión 3).
- Publicar en Google Play (firma, ficha, políticas): es una fase posterior de distribución, no de producto.
- MIDI, efectos, comping, automatización: fases futuras, no bloqueantes.
- Portar a iOS.
- Limpieza de las skins muertas (bandlab/reaper/draft): requiere permiso explícito del usuario.
- Revisión en hardware móvil real: no hay dispositivo (§5.1), así que queda como pendiente antes de publicar.
