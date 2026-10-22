#!/usr/bin/env bash
# Contratos del repo: las invariantes que, si se rompen, hacen que algo deje
# de funcionar. Solo Python: se puede correr en cualquier runner, sin Qt, sin
# el arbol de Ardour y sin compilar nada.
#
#   ./scripts/verify_contracts.sh
#
# Es la capa que puede correr en CI y en la verificacion post-agente del
# workflow de OpenCode. Comprueba, entre otras cosas:
#
#   - que Oboe llega por FetchContent y no por un find_package imposible
#   - que Gradle no compila el nativo (no hay externalNativeBuild)
#   - que el CMakeLists obsoleto de android/app/src/main/cpp/ no ha vuelto
#   - que build_android.sh sabe hacer APK Debug
#   - que existe AGENTS.md con la guia del repo
#
# Salida: 0 si todo esta bien, 1 si algo falla.

set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT}"

ROJO='\033[0;31m'
VERDE='\033[0;32m'
SIN_COLOR='\033[0m'

if [ ! -t 1 ]; then
    ROJO=''
    VERDE=''
    SIN_COLOR=''
fi

CONTRATOS=(
    "tests/check_qml_contract.py"
    "tests/check_android_build_script.py"
    "tests/check_spike_contract.py"
    "tests/check_android_pipeline.py"
)

FALLOS=0

for contrato in "${CONTRATOS[@]}"; do
    if [ ! -f "${contrato}" ]; then
        echo -e "${ROJO}[FALLO]${SIN_COLOR} ${contrato} no existe"
        FALLOS=$((FALLOS + 1))
        continue
    fi
    if python3 "${contrato}"; then
        echo -e "${VERDE}[OK]${SIN_COLOR}    ${contrato}"
    else
        echo -e "${ROJO}[FALLO]${SIN_COLOR} ${contrato}"
        FALLOS=$((FALLOS + 1))
    fi
done

if [ "${FALLOS}" -ne 0 ]; then
    echo ""
    echo -e "${ROJO}${FALLOS} contrato(s) en rojo.${SIN_COLOR}"
    exit 1
fi

echo ""
echo -e "${VERDE}Contratos en verde.${SIN_COLOR}"
exit 0
