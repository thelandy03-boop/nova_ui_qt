#!/bin/bash
# Compila y ejecuta los tests unitarios de nova_ui_qt.
# Uso: ./tests/run_tests.sh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/tests"
mkdir -p "${BUILD_DIR}"

fail=0

echo "== Test 1/3: politica de proteccion de proyectos =="
g++ -std=c++17 -fPIC -O0 -g \
    -I"${ROOT_DIR}/src" \
    $(pkg-config --cflags Qt6Core) \
    "${SCRIPT_DIR}/test_project_policy.cpp" \
    "${ROOT_DIR}/src/core/NovaProjectPolicy.cpp" \
    -o "${BUILD_DIR}/test_project_policy" \
    $(pkg-config --libs Qt6Core)
"${BUILD_DIR}/test_project_policy" || fail=1

echo
echo "== Test 2/3: calculo de picos (SIMD) =="
g++ -std=c++17 -fPIC -O0 -g \
    -I"${ROOT_DIR}/src" \
    "${SCRIPT_DIR}/test_simd_peaks.cpp" \
    "${ROOT_DIR}/src/core/NovaSimdPeaks.cpp" \
    -o "${BUILD_DIR}/test_simd_peaks"
"${BUILD_DIR}/test_simd_peaks" || fail=1

echo
echo "== Test 3/3: contrato QML <-> C++ =="
python3 "${SCRIPT_DIR}/check_qml_contract.py" || fail=1

echo
if [ "$fail" -eq 0 ]; then
    echo "== TODOS LOS TESTS PASAN =="
else
    echo "== HAY TESTS QUE FALLAN =="
fi
exit $fail
