# Spike: YSFX en Android

Banco de pruebas para decidir si **YSFX** puede sustituir al motor JSFX propio
de Nova. No forma parte del build de la app: no se enlaza con Qt, con Ardour ni
con Oboe. Es un ejecutable autónomo que se compila igual en Linux y para
Android.

## Qué se quiere demostrar

Los cinco criterios de salida que se fijaron antes de empezar:

1. `libysfx.a` compila para `arm64-v8a` y `x86_64` con NDK 26.1,
   `YSFX_PORTABLE=ON`, `YSFX_GFX=ON` y `YSFX_PLUGIN=OFF`.
2. Carga un script con sliders, `@sample` y `@serialize`.
3. Procesa audio estéreo a 48 kHz y el resultado es correcto.
4. `ysfx_gfx_run` pinta sobre un framebuffer BGRA que QML puede mostrar como
   `QImage`.
5. No arrastra JUCE ni una capa de ventanas (X11/GDK); solo LICE, stb y WDL.

Además se comprueban `@import`, las funciones de cadena y MIDI, porque son
justo las carencias del motor propio.

## Estructura

| Ruta | Qué es |
|---|---|
| `harness/main.cpp` | Banco de pruebas: audio, estado, gfx, import, strings y MIDI |
| `scripts/*.jsfx` | Scripts sintéticos que ejercitan cada función |
| `CMakeLists.txt` | Añade YSFX como subdirectorio con las opciones del spike |
| `herramientas/traer_ysfx.sh` | Clona YSFX en `.cache/ysfx` y aplica el parche local |
| `parches/` | Parches candidatos sobre YSFX/WDL, con su justificación |
| `docs/gfx-android.png` | Framebuffer de `@gfx` capturado en el emulador |
| `compilar_host.sh` | Compila y ejecuta en Linux |
| `compilar_android.sh` | Cross-compila el ejecutable para Android |
| `ejecutar_android.sh` | Empuja y ejecuta en un dispositivo o emulador |

## Cómo se usa

```bash
# Linux
./compilar_host.sh

# Android (x86_64 para emulador/Waydroid, arm64-v8a para teléfono)
./compilar_android.sh --abi x86_64
./ejecutar_android.sh --abi x86_64

# Teléfono real
./compilar_android.sh --abi arm64-v8a
./ejecutar_android.sh --abi arm64-v8a
```

El ejecutable imprime `[ OK ]` / `[FALLO]` por cada comprobación, vuelca el
framebuffer de `@gfx` a `gfx.ppm` y termina con código distinto de cero si algo
falla.

## Decisiones del spike

- **Commit fijado**: `5c3452fee62583aa3d1b7e877d0c758c4024af89` de
  `JoepVanlier/ysfx` (16 de agosto de 2026). No se sigue una rama.
- **No se vendoriza todavía**: el checkout vive en `.cache/ysfx`, ignorado por
  git. Si el spike pasa, se copia a `third_party/ysfx` con su README de origen.
- **Parche local sobre WDL**: `parches/0001-wdl-p1-null.patch` corrige un
  puntero nulo del intérprete EEL2 portable que hacía caer `Soft-Bell.jsfx`
  (ver más abajo). `traer_ysfx.sh` lo aplica de forma idempotente. No se ha
  enviado upstream.
- **`YSFX_SKIP_CHECKSUM=ON`**: el chequeo de checksum de YSFX usa
  `CMAKE_SOURCE_DIR` y falla cuando la librería se consume como
  subdirectorio. Además, con el parche local en WDL el checksum dejaría de
  coincidir: desactivarlo es requisito, no solo un atajo de compilación. No
  afecta al binario.
- **STL estática en Android**: `c++_static` para empujar un solo ejecutable,
  sin `libc++_shared.so` al lado.

## Licencia

YSFX es **Apache-2.0**. La obra combinada con Nova pasaría a **GPL-3.0**. El
plugin VST/CLAP de YSFX es AGPL-3.0, pero aquí se compila solo la librería
(`YSFX_PLUGIN=OFF`). WDL, LICE y stb son zlib o equivalentes; dr_libs es una
licencia permisiva (MIT/Unlicense según el fichero).

## Resultados

Fecha de la ejecución: 5 de noviembre de 2026.

### Criterios de salida

| # | Criterio | Estado |
|---|---|---|
| 1 | `libysfx.a` para `arm64-v8a` y `x86_64` con NDK 26.1 | ✅ ambas ABIs compilan |
| 2 | Carga sliders, `@sample` y `@serialize` | ✅ |
| 3 | Audio estéreo a 48 kHz correcto | ✅ |
| 4 | `@gfx` pinta en framebuffer BGRA | ✅ (texto incluido) |
| 5 | Sin JUCE ni X11/GDK; solo LICE, stb y WDL | ✅ |

### Linux (host)

```
Comprobaciones: 49, fallos: 0
rendimiento: 16.3 ns/muestra (61.5 Msps estéreo)
```

Texto renderizado con Fontconfig/FreeType del sistema.

### Android x86_64 (emulador)

Emulador `Medium_Phone_API_37.0`: Android 17 (API 37), ABI `x86_64`.
Binario compilado con `ANDROID_PLATFORM=android-31`, NDK 26.1.

```
Comprobaciones: 49, fallos: 0
rendimiento: 109.7 ns/muestra (9.1 Msps estéreo)
```

El texto **sí se renderiza** sin Fontconfig ni FreeType: LICE usa su fuente de
respaldo. El framebuffer resultante está en
[`docs/gfx-android.png`](docs/gfx-android.png).

Dependencias del ejecutable: `libdl.so`, `libm.so`, `libc.so`. Cero símbolos
`juce::`, cero referencias a GDK/X11. Tamaño: 8,5 MB sin strip (1,06 MB de
texto). El tamaño bajará al enlazar solo la librería en el `.so` de Nova y
aplicar `strip`.

### Android arm64-v8a

Compila correctamente, pero **no se ha ejecutado**: en este entorno no hay
teléfono conectado ni imagen de emulador arm64. La ABI se valida con `file` y
`readelf`; el comportamiento en un móvil real queda pendiente.

SHA-256 de los ejecutables de esta ejecución:

```
d181c759003d4ca071a6e4f17f83c5e98f6dd97ee72fcad70a6fc1552cebee32  x86_64
f0077c8d80799ab13639ae87f8b1b784ff728e3d8835de7be65bab44707a939a  arm64-v8a
```

### Corpus de scripts reales

Para no medir solo scripts sintéticos, `herramientas/medir_corpus.sh` descarga
seis repositorios públicos fijados a un commit y ejecuta 35 scripts reales de
la comunidad. Cada script se lanza en su propio proceso con un límite de 25 s:
un script roto no puede tumbar la medición entera.

| Repositorio | Commit | Scripts | Cargan y compilan | Con `@gfx` pintado |
|---|---|---:|---:|---:|
| `JoepVanlier/JSFX` | `c1c98b5f` | 10 | 10 | 7 |
| `geraintluff/jsfx` | `e98d6440` | 7 | 7 | 7 |
| `chkhld/jsfx` | `f6d175d8` | 6 | 6 | 2 |
| `Justin-Johnson/ReJJ` | `956ba452` | 2 | 2 | 2 |
| `JClones/JSFXClones` | `88a1503d` | 5 | 5 | 0 (no tienen `@gfx`) |
| `tiagolr/tilr_jsfx` | `cd533753` | 5 | 5 | 2 |
| **Total** | | **35** | **35** | **20** |

Los 35 scripts cargan, compilan y producen audio finito (sin NaN ni infinitos),
y los 20 que tienen `@gfx` pintan de verdad: ninguno queda con el framebuffer
sin cambios. El detalle por script está en `build-host/corpus.tsv`; el registro
de errores (`build-host/corpus_errores.log`) queda vacío.

### El caso de `Soft-Bell.jsfx`

Fue el único script que falló en la primera pasada, y no por carga ni por
audio: el SIGSEGV ocurría al ejecutar `@gfx`, dentro de `GLUE_CALL_CODE`
(`glue_port.h`, caso `EEL_BC_PUSH_VAL_AT_P1_TO_FPSTACK`). El bytecode generado
convertía el valor de retorno de un bloque con `*p1` cuando `p1` valía `NULL`.

Lo que se aisló:

- Solo pasa con el `@sample` completo compilado. Si se sustituye por dos
  asignaciones triviales, el mismo `@gfx` funciona. Da igual cuántas muestras
  se procesen: con `--frames 0` también falla.
- Pasa en Release con GCC 14 (`-O1`, `-O2` y `-O3`) y también con Clang 19 en
  el host. No pasa en `-O0` ni con ASan, que es justo lo que despista: el
  bytecode es el mismo, pero el valor que deja `p1` al entrar en la función
  depende del camino y de la optimización.
- La llamada que lo dispara es `control_group(-1)` de `ui-lib.jsfx-inc`, pero
  el mismo `control_group` funciona en un script mínimo que importa la misma
  librería. Hace falta el conjunto del script.
- **En Android x86_64 (NDK Clang, Release) funcionaba ya sin parche**. Eso
  descarta que el script o YSFX estén mal: es un defecto del intérprete
  portable de EEL2 al convertir el retorno de un bloque que no deja un puntero
  válido en `p1`.

El parche `0001-wdl-p1-null.patch` devuelve `0` en vez de desreferenciar un
`p1`/`p2`/`p3` nulo. No cambia la semántica de los scripts que sí dejan un
puntero válido. Con él, el corpus pasa de 34/35 a 35/35 en el host y el script
sigue funcionando en Android. UBSan, además, señala escrituras desalineadas en
el generador de bytecode portable (el flujo de opcodes usa enteros de 4 bytes
delante de punteros de 8); es UB real, pero x86_64 y arm64 toleran esos
accesos y no es la causa del puntero nulo.

### Scripts reales en el emulador

Además de los 49 checks sintéticos, se ejecutaron en el emulador x86_64 ocho
scripts del corpus, elegidos por ser los más exigentes: `Soft-Bell`,
`Level Meter`, `Echo-Cycles`, `ReEQ` (57 sliders), `ReSpectrum`, `js3osc`
(44 sliders), `saike_duskverb` y `wave_scope`. Los ocho cargan, compilan,
producen audio finito y pintan `@gfx`.

## Qué demuestra y qué no

**Demuestra** que YSFX es una librería embebible de verdad: compila para las
dos ABIs de Android, carga scripts reales, procesa audio, guarda estado,
pinta `@gfx` en un buffer que Qt puede envolver en `QImage`, y no arrastra
JUCE ni una capa de ventanas.

**No demuestra** todavía:

- Que el framebuffer se vea dentro de una app Qt Quick real (falta el puente
  `QImage`/`QQuickItem` y los eventos táctiles).
- El rendimiento en un teléfono real: el emulador x86_64 no es representativo
  de un ARM de gama media.
- Que `@gfx` complejo (menús, arrastrar y soltar, imágenes cargadas con
  `gfx_loadimg`) se comporte como en REAPER. En el corpus sí se han visto
  interfaces complejas construidas con `gfx_*` (ReEQ, ReSpectrum, js3osc,
  Level Meter, Echo-Cycles).
- Que un script aguante horas de sesión sin fugas ni deriva de estado.

## Límites conocidos de YSFX

- `gfx_setfont` depende de FreeType/Fontconfig en Linux y de SWELL en el resto.
  En Android no hay Fontconfig ni las fuentes en `/usr/share/fonts`. En este
  spike el texto salió con la fuente de respaldo de LICE; si un script usa una
  tipografía concreta, no la tendrá.
- `export_buffer_to_project`, `get_host_placement` y el mapeo de pines son
  stubs en YSFX: el script compila, pero no hacen nada.
- `filename:` de la cabecera no carga imágenes por sí solo: el script tiene que
  llamar a `gfx_loadimg`. Un `gfx_blit` sobre una imagen no cargada no pinta y
  tampoco revienta.
- El parche `0001-wdl-p1-null.patch` es local. Si se adopta YSFX hay que
  mantenerlo en `third_party/ysfx` (con su README y el checksum desactivado) o
  conseguir que upstream lo acepte. Sin él, `Soft-Bell.jsfx` cae en el host.
- Sin probar en un teléfono real en este entorno; el emulador valida la ABI y
  el runtime, no el rendimiento ni el audio real.

