#!/bin/bash
# Compila y ejecuta los tests unitarios de nova_ui_qt.
# Uso: ./tests/run_tests.sh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/tests"
mkdir -p "${BUILD_DIR}"

echo "== Compilando tests =="
g++ -std=c++17 -fPIC -O0 -g \
    -I"${ROOT_DIR}/src" \
    $(pkg-config --cflags Qt6Core) \
    "${SCRIPT_DIR}/test_project_policy.cpp" \
    "${ROOT_DIR}/src/core/NovaProjectPolicy.cpp" \
    -o "${BUILD_DIR}/test_project_policy" \
    $(pkg-config --libs Qt6Core)

echo "== Ejecutando tests =="
"${BUILD_DIR}/test_project_policy"
