#!/usr/bin/env bash
# Verificacion completa de NOVA Studio: lo que hay que ejecutar antes de abrir
# un Pull Request.
#
#   ./scripts/verify.sh
#
# Son dos capas:
#
#   1. contratos .py  -> las invariantes del repo leidas como texto
#   2. ctest          -> los tests de C++ (motor de audio, picos, QML)
#
# La capa 1 es la que avisa de que el camino de build se ha roto (por ejemplo,
# si alguien anade un bloque externalNativeBuild al build.gradle de Gradle o
# reintroduce el CMakeLists obsoleto de android/app/src/main/cpp/). Es la que
# puede correr en cualquier sitio, sin Qt ni el arbol de Ardour.
#
# La capa 2 necesita el arbol de Ardour completo (esta fuera del repo), asi
# que solo se puede correr en una maquina preparada. Si el build no esta
# configurado, esta capa se avisa y se salta: los contratos siguen siendo
# obligatorios.
#
# Si algo falla, el script lo dice con el nombre exacto de la comprobacion.
# No digas "deberia funcionar": esto es lo que dice si funciona.

set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT}"

BUILD_DIR="${BUILD_DIR:-build}"
FALLOS=0

ROJO='\033[0;31m'
VERDE='\033[0;32m'
AMARILLO='\033[0;33m'
SIN_COLOR='\033[0m'

if [ ! -t 1 ]; then
    ROJO=''
    VERDE=''
    AMARILLO=''
    SIN_COLOR=''
fi

titulo() {
    echo ""
    echo "=============================================================="
    echo "  $1"
    echo "=============================================================="
}

fallo() {
    FALLOS=$((FALLOS + 1))
    echo -e "${ROJO}[FALLO]${SIN_COLOR} $1"
}

acierto() {
    echo -e "${VERDE}[OK]${SIN_COLOR}    $1"
}

aviso() {
    echo -e "${AMARILLO}[AVISO]${SIN_COLOR} $1"
}

# ---------------------------------------------------------------------------
# 1. Contratos del repo (Python puro: se pueden correr en cualquier sitio)
# ---------------------------------------------------------------------------

titulo "1/2  Contratos del repo (invariantes)"

if ! "${ROOT}/scripts/verify_contracts.sh"; then
    fallo "los contratos del repo fallan"
fi

# ---------------------------------------------------------------------------
# 2. Tests de C++ (ctest): necesitan el arbol de Ardour, no siempre disponible
# ---------------------------------------------------------------------------

titulo "2/2  Tests de C++ (ctest)"

if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ]; then
    aviso "No hay build en '${BUILD_DIR}': se configura y se compila."
    if cmake -S . -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Debug > /dev/null; then
        cmake --build "${BUILD_DIR}" -j"$(nproc)" > /dev/null
    else
        fallo "cmake -S . -B ${BUILD_DIR} no pudo configurar el proyecto"
    fi
fi

if [ -f "${BUILD_DIR}/CMakeCache.txt" ]; then
    if ctest --test-dir "${BUILD_DIR}" --output-on-failure; then
        acierto "ctest: todos los tests en verde"
    else
        fallo "ctest: hay tests en rojo (mira el detalle arriba)"
    fi
else
    fallo "No se pudo configurar el proyecto: no se pueden correr los tests"
fi

# ---------------------------------------------------------------------------
# Resumen
# ---------------------------------------------------------------------------

titulo "Resumen"

if [ "${FALLOS}" -eq 0 ]; then
    echo -e "${VERDE}TODO EN VERDE.${SIN_COLOR} Se puede abrir el Pull Request."
    exit 0
fi

echo -e "${ROJO}${FALLOS} comprobacion(es) en rojo.${SIN_COLOR} Arregla lo de arriba antes de subir nada."
exit 1
