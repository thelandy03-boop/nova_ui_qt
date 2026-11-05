#!/bin/bash
# Trae el checkout de YSFX que usa el spike.
#
# Se fija un commit concreto (no una rama) para que el resultado sea
# reproducible. El checkout vive en .cache/ysfx, fuera del control de
# versiones, y no se vendoriza en el repo: el spike todavía no ha decidido si
# YSFX entra en Nova.
set -euo pipefail

AQUI="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DESTINO="${AQUI}/.cache/ysfx"
REPO="https://github.com/JoepVanlier/ysfx.git"
COMMIT="5c3452fee62583aa3d1b7e877d0c758c4024af89"
PARCHE="${AQUI}/parches/0001-wdl-p1-null.patch"

if [ -f "${DESTINO}/include/ysfx.h" ]; then
    echo "✅ YSFX ya está en ${DESTINO}"
else
    rm -rf "${DESTINO}"
    mkdir -p "$(dirname "${DESTINO}")"
    echo "⬇️  Clonando YSFX en ${DESTINO}"
    git clone "${REPO}" "${DESTINO}"
    git -C "${DESTINO}" checkout "${COMMIT}"
fi

echo "📦 Inicializando el submódulo dr_libs"
git -C "${DESTINO}" submodule update --init --depth 1 thirdparty/dr_libs

# Parche local sobre WDL: evita desreferenciar un p1 nulo al convertir el
# valor de retorno de un bloque. Ver parches/README.md.
if git -C "${DESTINO}" apply --check "${PARCHE}" 2>/dev/null; then
    git -C "${DESTINO}" apply "${PARCHE}"
    echo "🩹 Aplicado $(basename "${PARCHE}")"
elif git -C "${DESTINO}" apply --reverse --check "${PARCHE}" 2>/dev/null; then
    echo "🩹 $(basename "${PARCHE}") ya estaba aplicado"
else
    echo "⚠️  No se pudo aplicar ${PARCHE}: el checkout de YSFX no es el esperado" >&2
    exit 1
fi

echo "✅ YSFX listo: $(git -C "${DESTINO}" rev-parse --short HEAD)"
