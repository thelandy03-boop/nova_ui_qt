# minimp3

Decodificador de MP3 de una sola cabecera, usado por `src/audio/NovaMp3Decoder.cpp`.

## Origen

- Proyecto: <https://github.com/lieff/minimp3>
- Copia local: `third_party/minimp3/minimp3.h`, **sin modificar**.
- Procede del arbol de Ardour (`libs/ardour/ardour/minimp3.h`), que es
  exactamente el mismo fichero que usa el motor de Ardour. Se copia en vez de
  descargarse para no depender de la red durante la compilacion y para
  quedarse con la version que ya esta probada en este proyecto.

## Version y licencia

- Version: minimp3 de Lieff (el fichero de Ardour no lleva numero de version
  propio; su cabecera apunta a <https://github.com/lieff/minimp3>).
- Licencia: **dominio publico (CC0)**. La cabecera del fichero dice que el
  autor ha dedicado la obra al dominio publico worldwide y la distribuye sin
  garantia: <http://creativecommons.org/publicdomain/zero/1.0/>.
  Al ser dominio publico, se puede usar, copiar y modificar libremente.

## Por que minimp3 y no mpg123 ni otra libreria

- **mpg123 no existe en Android.** El core de Ardour decodifica MP3 con
  mpg123, y esa dependencia no esta disponible para compilar para Android
  (Waydroid), que es el objetivo de este trabajo.
- **libsndfile tampoco sirve.** En Android solo estaria la version del
  sistema (1.2.2), sin libmad, es decir sin soporte de MP3.
- **minimp3 es una sola cabecera** (el proyecto lo describe como unas 200
  lineas de codigo), sin dependencias ni build propio, asi que no engorda el
  APK ni obliga a mantener un `FetchContent` mas en el proyecto.

## Como se usa

`src/audio/NovaMp3Decoder.cpp` la incluye con `MINIMP3_IMPLEMENTATION` y
`MINIMP3_FLOAT_OUTPUT` (las muestras salen directamente en coma flotante, en
el rango -1..1).

Los simbolos se renombran al incluirla (`mp3dec_decode_frame` ->
`nova_mp3dec_decode_frame`, etc.) porque Ardour tambien incluye esta misma
cabecera y exporta esos simbolos desde `libardour.so`: si el ejecutable de
Nova los definiera con el mismo nombre, el enlazador dinamico haria que las
llamadas internas de Ardour resolvieran a la copia de Nova y ambos decodificadores
se pisarian. El fichero vendorizado no se toca: el renombrado se hace con
macros en el punto de inclusion.
