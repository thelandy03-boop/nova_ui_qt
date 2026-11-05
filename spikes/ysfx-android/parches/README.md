# Parches candidatos sobre YSFX

Aquí viven los parches que el spike necesita aplicar sobre el checkout de
YSFX en `.cache/ysfx`. No se aplican solos: cada uno se prueba y, si se
adopta YSFX, se documenta en `third_party/ysfx` con su justificación.

## `0001-wdl-p1-null.patch`

**Fichero**: `thirdparty/WDL/source/WDL/eel2/glue_port.h`
**Commit de YSFX**: `5c3452fee62583aa3d1b7e877d0c758c4024af89`

`Soft-Bell.jsfx` provoca un SIGSEGV al ejecutar `@gfx` en el host, dentro de
`GLUE_CALL_CODE`, caso `EEL_BC_PUSH_VAL_AT_P1_TO_FPSTACK`, porque el bytecode
generado convierte el valor de retorno de un bloque con `*p1` y `p1` vale
`NULL` en ese punto.

En el intérprete portable `p1` arranca a `NULL` en cada llamada a
`GLUE_CALL_CODE` y solo se rellena cuando la expresión evaluada deja un
puntero válido. Un bloque que termina en una expresión sin valor (o en una
rama que no pasa por la asignación) puede llegar a la conversión de retorno
con `p1` a `NULL`. Desreferenciarlo es un fallo; devolver 0 es el
comportamiento seguro: la expresión no tiene valor que devolver.

El parche no cambia la semántica de los scripts que sí dejan un puntero
válido; solo convierte un acceso inválido en un valor 0.

```bash
patch -p1 -d .cache/ysfx < parches/0001-wdl-p1-null.patch
```
