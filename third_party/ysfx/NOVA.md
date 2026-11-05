# NOVA Studio: vendoring de YSFX

Este directorio contiene una copia del repositorio `JoepVanlier/ysfx`, vendorizada
para sustituir al motor JSFX propio (WDL) de Nova. Es código ajeno: no se
modifica salvo el parche documentado abajo, por el mismo motivo que `minimp3` y
`wdl`.

## Origen

- Repositorio: https://github.com/JoepVanlier/ysfx
- Commit fijado: `5c3452fee62583aa3d1b7e877d0c758c4024af89` (16 de agosto de 2026)
- Licencia: Apache-2.0. La obra combinada con Nova se distribuye bajo GPL-3.0
  (ver `/LICENSE` y `/NOTICE`).
- Fecha de vendoring: 5 de noviembre de 2026.

## Opciones de compilación usadas

- `YSFX_PLUGIN=OFF` (solo la librería; el plugin VST/CLAP es AGPL-3.0 y no se usa)
- `YSFX_GFX=ON` (necesario para `@gfx`)
- `YSFX_PORTABLE=ON` (sin JIT, válido en Android)
- `YSFX_TESTS=OFF`, `YSFX_TOOLS=OFF`, `YSFX_DEEP_STRIP=OFF`
- `YSFX_SKIP_CHECKSUM=ON` (el chequeo usa `CMAKE_SOURCE_DIR` y falla como
  subdirectorio; además, el parche local invalidaría el checksum)

## Modificación local

El intérprete portable de EEL2 puede desreferenciar un puntero nulo al
convertir el valor de retorno de un bloque (`GLUE_CALL_CODE`, caso
`EEL_BC_PUSH_VAL_AT_P1_TO_FPSTACK`). Lo sufre `Soft-Bell.jsfx` en el host.
El parche `patches/0001-wdl-p1-null.patch` devuelve `0` en vez de
desreferenciar `p1`/`p2`/`p3` nulos.

Aplicado a: `thirdparty/WDL/source/WDL/eel2/glue_port.h`.

Sin este parche, `Soft-Bell.jsfx` y scripts de la misma familia caen en el
host. En Android el fallo era latente (dependía del camino de ejecución).

Ver el spike `spikes/ysfx-android/` para la evidencia completa y el banco de
pruebas que validó el parche.
