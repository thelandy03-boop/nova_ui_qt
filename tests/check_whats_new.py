#!/usr/bin/env python3
"""Comprueba que las novedades de la version actual esten documentadas en whats_new.json.

Regla obligatoria de release:
Antes de compilar, empaquetar o publicar cualquier release, es obligatorio que
la version configurada en android/app/build.gradle (versionName) este registrada
en qml/dialogs/whats_new.json con titulo, descripcion y al menos una novedad
o caracteristica detallada.

Tambien verifica que src/core/NovaUpdateChecker.h mantenga su version por defecto
alineada con la declarada en Gradle, evitando desincronizaciones en tiempo de ejecucion.

Uso: python3 tests/check_whats_new.py
"""
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
BUILD_GRADLE = ROOT / "android" / "app" / "build.gradle"
WHATS_NEW_JSON = ROOT / "qml" / "dialogs" / "whats_new.json"
UPDATE_CHECKER_H = ROOT / "src" / "core" / "NovaUpdateChecker.h"


def obtener_version_gradle() -> str:
    """Extrae versionName del build.gradle de Android."""
    if not BUILD_GRADLE.exists():
        raise FileNotFoundError(f"No se encontro {BUILD_GRADLE}")
    contenido = BUILD_GRADLE.read_text(encoding="utf-8")
    m = re.search(r'versionName\s+["\']([^"\']+)["\']', contenido)
    if not m:
        raise ValueError(f"No se pudo extraer versionName de {BUILD_GRADLE}")
    return m.group(1).strip()


def obtener_version_update_checker() -> str:
    """Extrae m_currentVersion por defecto de NovaUpdateChecker.h."""
    if not UPDATE_CHECKER_H.exists():
        raise FileNotFoundError(f"No se encontro {UPDATE_CHECKER_H}")
    contenido = UPDATE_CHECKER_H.read_text(encoding="utf-8")
    m = re.search(r'm_currentVersion\s*=\s*QStringLiteral\(["\']([^"\']+)["\']\)', contenido)
    if not m:
        raise ValueError(f"No se pudo extraer m_currentVersion de {UPDATE_CHECKER_H}")
    return m.group(1).strip()


def verificar_whats_new(version_actual: str) -> None:
    """Verifica que whats_new.json contenga una entrada valida y completa para version_actual."""
    if not WHATS_NEW_JSON.exists():
        raise FileNotFoundError(f"No existe el archivo obligatorio {WHATS_NEW_JSON}")

    try:
        data = json.loads(WHATS_NEW_JSON.read_text(encoding="utf-8"))
    except json.JSONDecodeError as err:
        raise ValueError(f"Error de sintaxis JSON en {WHATS_NEW_JSON}: {err}") from err

    releases = data.get("releases")
    if not isinstance(releases, dict):
        raise ValueError(f"{WHATS_NEW_JSON} debe contener un objeto raiz 'releases'")

    if version_actual not in releases:
        ejemplo = (
            f'"{version_actual}": {{\n'
            f'  "updateBadge": "✨ ACTUALIZACIÓN",\n'
            f'  "title": "¿Qué hay de nuevo en Nova?",\n'
            f'  "description": "Resumen de los cambios de esta versión:",\n'
            f'  "buttonText": "¡Empezar a Crear! ➜",\n'
            f'  "features": [\n'
            f'    {{\n'
            f'      "category": "NUEVO",\n'
            f'      "icon": "⚡",\n'
            f'      "title": "Nombre de la novedad",\n'
            f'      "description": "Detalle descriptivo de la funcionalidad."\n'
            f'    }}\n'
            f'  ]\n'
            f'}}'
        )
        raise AssertionError(
            f"\n"
            f"❌ ERROR: La versión actual '{version_actual}' configurada en android/app/build.gradle\n"
            f"   NO está documentada en qml/dialogs/whats_new.json.\n\n"
            f"   Es OBLIGATORIO registrar las novedades antes de publicar o compilar un release.\n"
            f"   Añade una entrada como esta dentro de 'releases' en {WHATS_NEW_JSON}:\n\n"
            f"{ejemplo}\n"
        )

    rel = releases[version_actual]
    if not isinstance(rel, dict):
        raise AssertionError(f"La entrada 'releases.{version_actual}' debe ser un objeto JSON")

    title = rel.get("title", "").strip()
    if not title:
        raise AssertionError(f"La versión '{version_actual}' en whats_new.json debe incluir un 'title' no vacío")

    desc = rel.get("description", "").strip()
    if not desc:
        raise AssertionError(f"La versión '{version_actual}' en whats_new.json debe incluir una 'description' no vacía")

    features = rel.get("features")
    if not isinstance(features, list) or len(features) == 0:
        raise AssertionError(
            f"La versión '{version_actual}' en whats_new.json debe incluir una lista 'features' con al menos 1 elemento"
        )

    for idx, feat in enumerate(features, 1):
        if not isinstance(feat, dict):
            raise AssertionError(f"Feature #{idx} en '{version_actual}' debe ser un objeto")
        f_title = feat.get("title", "").strip()
        f_desc = feat.get("description", "").strip()
        f_cat = feat.get("category", "").strip()
        if not f_title or not f_desc or not f_cat:
            raise AssertionError(
                f"Feature #{idx} en '{version_actual}' debe tener 'category', 'title' y 'description' no vacíos"
            )


def main() -> int:
    print("== Contrato de Novedades de Release (whats_new.json) ==")
    try:
        ver_gradle = obtener_version_gradle()
        print(f"   Versión en android/app/build.gradle: {ver_gradle}")

        ver_checker = obtener_version_update_checker()
        print(f"   Versión en NovaUpdateChecker.h:      {ver_checker}")

        if ver_gradle != ver_checker:
            raise AssertionError(
                f"Desincronización de versiones:\n"
                f"  - android/app/build.gradle: {ver_gradle}\n"
                f"  - NovaUpdateChecker.h:      {ver_checker}\n"
                f"Ambos archivos deben tener la misma versión actual."
            )

        verificar_whats_new(ver_gradle)
        print(f"   [OK] qml/dialogs/whats_new.json contiene novedades válidas para la v{ver_gradle}")
        print("\n   OK: el contrato de novedades de release se cumple estrictamente.")
        return 0
    except Exception as exc:
        print(f"\n[FALLO] {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
