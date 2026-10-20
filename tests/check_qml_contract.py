#!/usr/bin/env python3
"""Comprueba que todo lo que el QML llama a la API de C++ exista de verdad.

Un boton que llama a un metodo inexistente no da error de compilacion:
falla en runtime con "TypeError: Property 'x' of object ... is not a
function" y el boton queda muerto sin avisar. Este test lo detecta antes.

Uso: python3 tests/check_qml_contract.py
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "src"
QML = ROOT / "qml"

# Objetos de C++ accesibles desde el QML y la clase que los implementa.
OBJECTS = {
    "AudioEngine": SRC / "NovaAudioEngine.h",
    "AudioEngine.tracks": SRC / "models" / "NovaTrackListModel.h",
    "AudioEngine.regions": SRC / "models" / "NovaRegionModel.h",
    "AudioEngine.deviceManager": SRC / "core" / "NovaAudioDeviceManager.h",
}

# Metodos que heredan de QAbstractItemModel y por tanto existen en el modelo.
INHERITED_MODEL = {"rowCount", "data", "index", "parent", "mapToSource", "hasIndex"}


def exposed_members(header: pathlib.Path) -> set:
    """Miembros publicos expuestos a QML: Q_PROPERTY, Q_INVOKABLE y metodos publicos."""
    text = header.read_text(errors="ignore")
    members = set()

    # Q_PROPERTY( Tipo nombre READ getter ...) - el tipo puede llevar '*' o '&'
    for m in re.finditer(r"Q_PROPERTY\s*\(\s*[^,]+?([A-Za-z_]\w*)\s+READ\s+(\w+)", text):
        members.add(m.group(1))
        members.add(m.group(2))

    # Q_INVOKABLE [const] tipo nombre(...)
    for m in re.finditer(r"Q_INVOKABLE\s+[^;{]*?\b(\w+)\s*\(", text):
        members.add(m.group(1))

    # Metodos publicos declarados en la clase (nombre + parentesis + ';')
    for m in re.finditer(r"^\s{4}(?:[\w:<>,\s\*&]+?)\b(\w+)\s*\([^;{]*\)\s*(?:const)?\s*;", text, re.M):
        members.add(m.group(1))

    return members


def qml_usages():
    """Devuelve {objeto: {miembro: [ficheros]}}"""
    usages = {}
    for path in sorted(QML.rglob("*.qml")):
        text = path.read_text(errors="ignore")
        for obj in sorted(OBJECTS, key=len, reverse=True):
            for m in re.finditer(re.escape(obj) + r"\.(\w+)", text):
                member = m.group(1)
                usages.setdefault(obj, {}).setdefault(member, set()).add(
                    str(path.relative_to(ROOT))
                )
    return usages


def main() -> int:
    exposed = {obj: exposed_members(path) for obj, path in OBJECTS.items()}
    usages = qml_usages()

    missing = []
    total = 0

    for obj, members in usages.items():
        for member, files in members.items():
            total += 1
            if member in exposed[obj] or member in INHERITED_MODEL:
                continue
            missing.append((obj, member, sorted(files)))

    print(f"== Contrato QML <-> C++ ==")
    print(f"   {total} usos de API comprobados en {len(usages)} objetos\n")

    if missing:
        print("   !! MIEMBROS QUE EL QML USA PERO QUE NO EXISTEN EN C++:")
        for obj, member, files in missing:
            print(f"      {obj}.{member}   en {len(files)} fichero(s): {files[:3]}")
        print("\n   Esos botones/controles fallan en runtime con TypeError.")
        return 1

    print("   OK: todo lo que el QML usa existe en la API de C++")
    return 0


if __name__ == "__main__":
    sys.exit(main())
