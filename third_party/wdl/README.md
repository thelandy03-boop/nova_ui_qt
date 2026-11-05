# WDL (Cockos): motor EEL2

Copia vendorizada, **sin modificar**, de la parte de WDL necesaria para
ejecutar el lenguaje EEL2 que usan los scripts JSFX de Cockos REAPER.

No es una reimplementación ni un resumen: son los ficheros originales del
repositorio de WDL, con sus cabeceras de copyright y licencia intactas.

## Origen

- Proyecto: <https://github.com/justinfrankel/WDL>
- Commit copiado: `03c6e56` (`virtwnd-iconbutton: fix retina drawing glitch...`)
- Fecha de la copia: 2026-11-04
- Rutas originales:
  - `WDL/eel2/`
  - `WDL/fft.c`
  - `WDL/fft.h`
  - `WDL/wdltypes.h`
  - `WDL/denormal.h`
  - `WDL/wdlcstring.h`
  - `WDL/utf8_extended.h`

La copia se verificó con `cmp` byte a byte contra el árbol clonado. Ningún
fichero se ha editado, parcheado ni renombrado.

## Qué hay y para qué sirve

| Fichero | Papel |
|---|---|
| `eel2/ns-eel.h` | API pública del compilador y de la VM EEL2 |
| `eel2/ns-eel-int.h` | Tipos internos del compilador |
| `eel2/ns-eel-addfuncs.h` | Macros para registrar funciones del host |
| `eel2/glue_port.h` | Backend portable (sin generar código ejecutable) |
| `eel2/y.tab.c`, `eel2/y.tab.h` | Parser generado por Bison, incluido por `nseel-yylex.c` |
| `eel2/nseel-compiler.c` | Compilador EEL2 |
| `eel2/nseel-eval.c` | Gestión de la VM (`NSEEL_VM_alloc`, `NSEEL_VM_free`) |
| `eel2/nseel-ram.c` | Memoria RAM de los scripts (`memset`, `memcpy`, `freembuf`) |
| `eel2/nseel-yylex.c`, `nseel-lextab.c`, `nseel-caltab.c`, `nseel-cfunc.c` | Lexer, tablas y plantillas de operadores |
| `eel2/eel_fft.h` | Funciones `fft`, `ifft`, `convolve_c` del host |
| `fft.c`, `fft.h` | FFT de WDL (base de `eel_fft.h`) |
| `wdltypes.h`, `denormal.h`, `wdlcstring.h`, `utf8_extended.h` | Dependencias mínimas del motor |

No se vendorizan `eel_strings.h`, `eel_files.h`, `eel_mdct.h`, `eel_lice.h`
ni el resto de WDL: no hacen falta para audio y arrastrarían dependencias que
esta fase no necesita (`@gfx`, ficheros, cadenas, MDCT, LICE).

## Cómo se compila

La librería estática `nova_wdl_eel2` la define `CMakeLists.txt` con:

- `EEL_TARGET_PORTABLE`: backend portable de WDL. No genera código máquina
  en tiempo de ejecución, así que funciona en Android sin depender de
  `mmap(PROT_EXEC)` ni de las restricciones de ejecución de código de las
  versiones recientes del sistema.
- `WDL_FFT_REALSIZE=8`: las muestras de EEL2 son `double`; la FFT debe usar
  el mismo tamaño.
- `-fsigned-char` en ARM: WDL usa `char` con signo en el lexer y el
  compilador. En ARM `char` es sin signo por defecto y sin este flag el
  análisis de ciertos literales cambia. El flag también se aplica a
  `NovaJsfxWdlEngine.cpp`, la única unidad C++ que incluye `wdltypes.h`:
  ese fichero comprueba en tiempo de compilación que `char` sea con signo.
- `_FILE_OFFSET_BITS=64` en plataformas de 32 bits.

`NSEEL_HOSTSTUB_EnterMutex` y `NSEEL_HOSTSTUB_LeaveMutex` los implementa
`src/audio/dsp/jsfx/NovaJsfxWdlEngine.cpp` con un mutex global, porque WDL
los necesita para proteger la memoria compartida entre VMs.

## Variables del host

`NovaJsfxWdlEngine` registra las variables que espera un JSFX: `srate`,
`samplesblock`, `num_ch`, `spl0`, `spl1`, los `sliderN` y el transporte
(`tempo`, `play_position`, `beat_position`, `play_state`).

El transporte lo publica `NovaJsfxEffect::setTransport`: la UI lo actualiza
a ~60 FPS desde la sesión (o desde el reproductor de Oboe en Android) y el
efecto lo aplica antes de `@block` y `@sample`. Sin transporte disponible,
`tempo` vale 120 y `play_position` avanza con el reloj interno del efecto.

## Licencia

Todos los ficheros de EEL2, FFT y las cabeceras auxiliares llevan en su
cabecera una licencia tipo **zlib** (uso comercial permitido, conservando el
aviso de copyright y sin atribuirse la autoría). El texto completo y los
titulares están en `LICENSE`.

`eel2/y.tab.c` y `eel2/y.tab.h` son la salida de GNU Bison 2.3. Llevan la
licencia GPLv2 **con la excepción especial de Bison**, que permite distribuir
el esqueleto del parser como parte de una obra mayor bajo los términos que
se elijan. El texto de esa licencia y de la excepción está en
`LICENSE-bison.txt`.

## Por qué se vendoriza y no se descarga con FetchContent

- El build de Android no debe depender de la red.
- La versión queda fijada a un commit concreto y se puede auditar.
- WDL no publica un `LICENSE` raíz; los avisos van en cada fichero y se
  conservan tal cual.
- Es el mismo criterio que ya se usa con `third_party/minimp3`.
