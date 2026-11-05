#!/bin/bash
# Compila y ejecuta el banco de pruebas de YSFX en Linux.
set -euo pipefail

AQUI="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="${AQUI}/build-host"

"${AQUI}/herramientas/traer_ysfx.sh"

cmake -S "${AQUI}" -B "${BUILD}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DYSFX_SOURCE_DIR="${AQUI}/.cache/ysfx"
cmake --build "${BUILD}" -j"$(nproc)"

"${BUILD}/nova_ysfx_spike" --scripts "${AQUI}/scripts" --out "${BUILD}"
