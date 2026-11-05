# Spike de YSFX en Android — banco de pruebas.
#
# Este proyecto NO forma parte del build de Nova: vive aparte para medir si
# YSFX es viable como motor JSFX. No se enlaza con Qt ni con Ardour.
#
# Uso:
#   ./herramientas/traer_ysfx.sh
#   cmake -S . -B build-host -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-host
#   ./build-host/nova_ysfx_spike --scripts ./scripts --out ./build-host

cmake_minimum_required(VERSION 3.21)
project(nova_ysfx_spike LANGUAGES C CXX)

set(YSFX_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.cache/ysfx"
    CACHE PATH "Ruta al checkout de YSFX (JoepVanlier/ysfx)")

if(NOT EXISTS "${YSFX_SOURCE_DIR}/include/ysfx.h")
    message(FATAL_ERROR
        "No se encuentra YSFX en '${YSFX_SOURCE_DIR}'.\n"
        "Ejecuta primero: ./herramientas/traer_ysfx.sh")
endif()

# Opciones fijadas por el spike: librería, portable y con @gfx.
set(YSFX_PLUGIN OFF CACHE BOOL "" FORCE)
set(YSFX_GFX ON CACHE BOOL "" FORCE)
set(YSFX_PORTABLE ON CACHE BOOL "" FORCE)
set(YSFX_TESTS OFF CACHE BOOL "" FORCE)
set(YSFX_TOOLS OFF CACHE BOOL "" FORCE)
set(YSFX_DEEP_STRIP OFF CACHE BOOL "" FORCE)
# El chequeo de checksum del ASM usa CMAKE_SOURCE_DIR (la raíz del proyecto que
# lo consume) y falla cuando YSFX se añade como subdirectorio. No afecta al
# binario: solo valida que la copia de WDL no se haya modificado.
set(YSFX_SKIP_CHECKSUM ON CACHE BOOL "" FORCE)

add_subdirectory("${YSFX_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/ysfx"
                 EXCLUDE_FROM_ALL)

add_executable(nova_ysfx_spike harness/main.cpp)
target_compile_features(nova_ysfx_spike PRIVATE cxx_std_17)
target_compile_definitions(nova_ysfx_spike PRIVATE
    NOVA_SPIKE_SCRIPTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/scripts")
target_link_libraries(nova_ysfx_spike PRIVATE ysfx)

# En Android el ejecutable se empuja con adb y se lanza desde una shell. Se
# enlaza la STL estáticamente para no tener que empujar libc++_shared.so.
if(ANDROID)
    target_link_options(nova_ysfx_spike PRIVATE "-static-libstdc++")
endif()
