#!/bin/bash
# Empuja y ejecuta el banco de pruebas en un dispositivo Android conectado
# (o en Waydroid, que expone adb igual que un teléfono).
#
# Uso: ./ejecutar_android.sh [--abi <x86_64|arm64-v8a>]
set -euo pipefail

AQUI="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ABI="x86_64"
DESTINO="/data/local/tmp/nova_ysfx_spike"

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

BIN="${AQUI}/build-android-${ABI}/nova_ysfx_spike"
if [ ! -x "${BIN}" ]; then
    echo "No existe ${BIN}. Ejecuta antes ./compilar_android.sh --abi ${ABI}" >&2
    exit 1
fi

if ! adb get-state >/dev/null 2>&1; then
    echo "No hay ningún dispositivo Android conectado (adb devices vacío)." >&2
    echo "Arranca Waydroid o conecta un teléfono con depuración USB." >&2
    exit 1
fi

echo "📲 Preparando ${DESTINO}"
adb shell "rm -rf ${DESTINO} && mkdir -p ${DESTINO}/scripts ${DESTINO}/out"
adb push "${BIN}" "${DESTINO}/nova_ysfx_spike" >/dev/null
adb push "${AQUI}/scripts/." "${DESTINO}/scripts/" >/dev/null
adb shell "chmod +x ${DESTINO}/nova_ysfx_spike"

echo "▶️  Ejecutando"
set +e
adb shell "cd ${DESTINO} && ./nova_ysfx_spike --scripts ${DESTINO}/scripts --out ${DESTINO}/out"
ESTADO=$?
set -e

adb pull "${DESTINO}/out/." "${AQUI}/build-android-${ABI}/salida-android" >/dev/null 2>&1 || true
echo "Salida copiada a ${AQUI}/build-android-${ABI}/salida-android"
exit ${ESTADO}
