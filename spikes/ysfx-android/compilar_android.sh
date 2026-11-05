#!/bin/bash
# Compila el banco de pruebas de YSFX para Android.
#
# No genera un APK: produce un ejecutable que se empuja con adb y se lanza por
# shell. Eso basta para medir carga, audio, @serialize, @gfx y MIDI sin
# montar todavía una app Qt.
#
# Uso: ./compilar_android.sh [--abi <x86_64|arm64-v8a>]
set -euo pipefail

AQUI="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ABI="x86_64"
NDK="${ANDROID_NDK_ROOT:-${HOME}/Android/Sdk/ndk/26.1.10909125}"

while [ $# -gt 0 ]; do
    case "$1" in
        --abi)
            ABI="${2:?--abi necesita un valor}"
            shift 2
            ;;
        -h|--help)
            echo "Uso: $(basename "$0") [--abi <x86_64|arm64-v8a>]"
            exit 0
            ;;
        *)
            echo "Opción desconocida: $1" >&2
            exit 1
            ;;
    esac
done

case "${ABI}" in
    x86_64|arm64-v8a) ;;
    *)
        echo "ABI no soportada: ${ABI}" >&2
        exit 1
        ;;
esac

if [ ! -f "${NDK}/build/cmake/android.toolchain.cmake" ]; then
    echo "No se encuentra el NDK 26.1 en ${NDK}" >&2
    echo "Exporta ANDROID_NDK_ROOT o instala la versión 26.1.10909125." >&2
    exit 1
fi

"${AQUI}/herramientas/traer_ysfx.sh"

BUILD="${AQUI}/build-android-${ABI}"
cmake -S "${AQUI}" -B "${BUILD}" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="${NDK}/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI="${ABI}" \
    -DANDROID_PLATFORM=android-31 \
    -DANDROID_STL=c++_static \
    -DCMAKE_BUILD_TYPE=Release \
    -DYSFX_SOURCE_DIR="${AQUI}/.cache/ysfx"
cmake --build "${BUILD}" -j"$(nproc)"

echo "✅ Ejecutable: ${BUILD}/nova_ysfx_spike"
file "${BUILD}/nova_ysfx_spike"
