#!/usr/bin/env python3
"""Comprueba que build_android.sh sabe compilar para el ABI que se le pida.

Un APK de Android solo se instala si el ABI que lleva dentro coincide con el
del dispositivo. El script fijaba arm64-v8a a pelo, de modo que en la imagen
de Waydroid (x86_64) noidia instalar nada. Este test fija el contrato del
parametro --abi: valores admitidos, valor por defecto, toolchain de Qt, flags
de CMake, ajuste del staging de Gradle y nombre del APK.

Seis comprobaciones leen el script como texto; las otras dos lo ejecutan de
verdad en una sandbox (HOME y directorio de trabajo temporales, con qt-cmake,
cmake y gradlew simulados) para comprobar el comportamiento y no solo que
existan las cadenas.

Uso: python3 tests/check_android_build_script.py
"""
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
SCRIPT = ROOT / "build_android.sh"

ABI_POR_DEFECTO = "arm64-v8a"
ABIS_ADMITIDOS = ("arm64-v8a", "x86_64")

# ---------------------------------------------------------------------------
# Stubs de las herramientas que invoca el script de build
# ---------------------------------------------------------------------------

# Simula a androiddeployqt: deja el staging del proyecto Android (copia su
# build.gradle), el cache de CMake con el ABI configurado y un wrapper de
# Gradle que produce el APK.
STUB_QT_CMAKE = """#!/bin/bash
set -e
echo "$0 $*" >> "${NOVA_TEST_LOG}"

BUILD_DIR="build_android"
prev=""
for arg in "$@"; do
    if [ "${prev}" = "-B" ]; then BUILD_DIR="${arg}"; fi
    prev="${arg}"
done

ABI=$(printf '%s\\n' "$@" | sed -n 's/.*-DANDROID_ABI=\\([^ ]*\\).*/\\1/p')

mkdir -p "${BUILD_DIR}/android-build/app/build/outputs/apk/release"
cp android/app/build.gradle "${BUILD_DIR}/android-build/app/build.gradle"
printf 'ANDROID_ABI:UNINITIALIZED=%s\\n' "${ABI}" > "${BUILD_DIR}/CMakeCache.txt"

cat > "${BUILD_DIR}/android-build/gradlew" <<'GRADLEW'
#!/bin/bash
set -e
mkdir -p app/build/outputs/apk/release
printf 'APK de prueba\\n' > app/build/outputs/apk/release/app-release.apk
GRADLEW
chmod +x "${BUILD_DIR}/android-build/gradlew"
"""

# El script llama a `cmake --build ...` dos veces: no hay nada que compilar.
STUB_CMAKE = """#!/bin/bash
echo "$0 $*" >> "${NOVA_TEST_LOG}"
exit 0
"""


def _escribir_stub(ruta: pathlib.Path, cuerpo: str) -> None:
    ruta.parent.mkdir(parents=True, exist_ok=True)
    ruta.write_text(cuerpo)
    ruta.chmod(0o755)


def _exigir(condicion: bool, mensaje: str) -> None:
    if not condicion:
        raise AssertionError(mensaje)


class Sandbox:
    """HOME, directorio de trabajo y stubs para ejecutar build_android.sh."""

    def __init__(self, raiz: pathlib.Path, con_qt: bool = True) -> None:
        self.raiz = raiz
        self.home = raiz / "home"
        self.trabajo = raiz / "trabajo"
        self.bin = raiz / "bin"
        self.log = raiz / "invocaciones.log"

        self.home.mkdir(parents=True)
        self.trabajo.mkdir(parents=True)
        self.bin.mkdir(parents=True)
        self.log.write_text("")

        # El proyecto Android real: el staging copia su build.gradle.
        shutil.copytree(ROOT / "android", self.trabajo / "android")

        _escribir_stub(self.bin / "cmake", STUB_CMAKE)
        if con_qt:
            for abi in ABIS_ADMITIDOS:
                kit = self.home / "Qt" / "6.8.3" / f"android_{abi.replace('-', '_')}"
                _escribir_stub(kit / "bin" / "qt-cmake", STUB_QT_CMAKE)
                (kit / "jar").mkdir(parents=True)
                (kit / "jar" / "qt-core.jar").write_bytes(b"jar de prueba\n")

    def ejecutar(self, *args: str) -> subprocess.CompletedProcess:
        """Ejecuta el script real en la sandbox."""
        env = os.environ.copy()
        env.pop("ABI", None)
        env["HOME"] = str(self.home)
        env["PATH"] = f"{self.bin}{os.pathsep}{env['PATH']}"
        env["NOVA_TEST_LOG"] = str(self.log)
        return subprocess.run(
            ["bash", str(SCRIPT), *args],
            cwd=str(self.trabajo),
            env=env,
            capture_output=True,
            text=True,
            timeout=120,
        )

    def invocaciones(self) -> str:
        return self.log.read_text()

    def apk(self, abi: str) -> pathlib.Path:
        return self.trabajo / "apks" / f"NovaStudio-{abi}.apk"

    def staging_gradle(self) -> pathlib.Path:
        return self.trabajo / "build_android" / "android-build" / "app" / "build.gradle"

    def lineas_de(self, fichero: pathlib.Path, patron: str) -> list:
        """Lineas del fichero que mencionan patron, sin sangria."""
        return [
            linea.strip()
            for linea in fichero.read_text().splitlines()
            if patron in linea
        ]


# ---------------------------------------------------------------------------
# Comprobaciones sobre el texto del script
# ---------------------------------------------------------------------------


def _codigo(texto: str) -> str:
    """El script sin comentarios: el contrato habla de codigo, no de prosa."""
    return "\n".join(
        linea for linea in texto.splitlines() if not linea.lstrip().startswith("#")
    )


def soporta_parametro_abi(texto: str) -> None:
    codigo = _codigo(texto)
    _exigir("--abi" in codigo, "el script no menciona el parametro --abi")
    _exigir(
        re.search(r"while\s+\[?\s*\$\#", codigo) is not None,
        "el script no recorre los argumentos de la linea de ordenes",
    )
    _exigir("shift" in codigo, "el script no consume los argumentos que procesa")


def abi_por_defecto_arm64(texto: str) -> None:
    codigo = _codigo(texto)
    _exigir(
        re.search(r'^\s*ABI\s*=\s*"\$\{ABI:-arm64-v8a\}"', codigo, re.M) is not None
        or re.search(r'^\s*ABI\s*=\s*"arm64-v8a"', codigo, re.M) is not None,
        "el script no define arm64-v8a como ABI por defecto",
    )


def toolchain_derivado_del_abi(texto: str) -> None:
    codigo = _codigo(texto)
    # La ruta se construye con una variable: android_${VARIABLE}
    rutas = re.findall(r"android_\$\{([^}]+)\}", codigo)
    _exigir(rutas, "la ruta del toolchain no se deriva de una variable")

    # Y esa variable tiene que derivarse del ABI, no ser un valor fijo.
    derivada = False
    for expresion in rutas:
        if re.search(r"\$\{ABI(?:\}|\/\/)", expresion):
            derivada = True
            break
        for linea in codigo.splitlines():
            if re.match(rf"\s*{re.escape(expresion)}\s*=", linea) and re.search(
                r"\$\{ABI(?:\}|\/\/)", linea
            ):
                derivada = True
                break
        if derivada:
            break
    _exigir(derivada, "la variable de la ruta del toolchain no viene del ABI")

    _exigir(
        "android_arm64_v8a" not in codigo and "android_arm64-v8a" not in codigo,
        "la ruta del toolchain sigue hardcodeada en vez de derivarse del ABI",
    )


def nombre_de_apk_incluye_abi(texto: str) -> None:
    _exigir(
        re.search(r'APK_DEST=.*NovaStudio-\$\{ABI\}', _codigo(texto)) is not None,
        "el APK de salida no se llama NovaStudio-<abi>.apk",
    )


def pasa_android_abi_a_cmake(texto: str) -> None:
    _exigir(
        re.search(r'-DANDROID_ABI="?\$\{ABI\}"?', _codigo(texto)) is not None,
        "el script no pasa -DANDROID_ABI con el ABI elegido",
    )


def ajusta_abifilters_del_staging(texto: str) -> None:
    codigo = _codigo(texto)
    _exigir(
        "android-build/app/build.gradle" in codigo,
        "el script no menciona el build.gradle del staging",
    )
    reescrituras = [
        linea
        for linea in codigo.splitlines()
        if "sed" in linea and "abiFilters" in linea
    ]
    _exigir(reescrituras, "el script no reescribe el abiFilters del staging")
    _exigir(
        any("${ABI}" in linea for linea in reescrituras),
        "el abiFilters del staging no se reescribe con el ABI elegido",
    )


# ---------------------------------------------------------------------------
# Comprobaciones sobre el comportamiento real del script
# ---------------------------------------------------------------------------


def rechaza_abi_invalido() -> None:
    """Un ABI no soportado debe abortar el script con un mensaje claro."""
    with tempfile.TemporaryDirectory() as tmp:
        sandbox = Sandbox(pathlib.Path(tmp), con_qt=False)
        proc = sandbox.ejecutar("--abi", "armeabi-v7a")
        salida = proc.stdout + proc.stderr
        _exigir(
            proc.returncode != 0,
            "el script deberia abortar con un ABI no soportado (codigo %d)" % proc.returncode,
        )
        _exigir(
            "armeabi-v7a" in salida,
            "el mensaje de error no menciona el ABI invalido",
        )
        _exigir(
            "arm64-v8a" in salida and "x86_64" in salida,
            "el mensaje de error no indica los ABIs admitidos",
        )


def genera_apk_con_el_abi_elegido() -> None:
    """El APK debe salir con el nombre y el contenido del ABI pedido."""
    with tempfile.TemporaryDirectory() as tmp:
        sandbox = Sandbox(pathlib.Path(tmp))

        # 1) Sin argumentos: arm64-v8a por defecto.
        proc = sandbox.ejecutar()
        _exigir(
            proc.returncode == 0,
            "el script fallo con el ABI por defecto:\n%s" % (proc.stdout + proc.stderr),
        )
        log = sandbox.invocaciones()
        _exigir(
            "android_arm64_v8a/bin/qt-cmake" in log,
            "sin argumentos no se usa el kit Qt android_arm64_v8a:\n%s" % log,
        )
        _exigir(
            "-DANDROID_ABI=arm64-v8a" in log,
            "sin argumentos no se pasa -DANDROID_ABI=arm64-v8a:\n%s" % log,
        )
        _exigir(
            sandbox.apk(ABI_POR_DEFECTO).is_file(),
            "sin argumentos no se genera apks/NovaStudio-%s.apk" % ABI_POR_DEFECTO,
        )
        staging = sandbox.staging_gradle()
        _exigir(
            sandbox.lineas_de(staging, "abiFilters") == ["abiFilters 'arm64-v8a'"],
            "el staging no queda con un unico abiFilters 'arm64-v8a':\n%s"
            % staging.read_text(),
        )

        # 2) Con --abi x86_64 (el ABI de la imagen de Waydroid).
        proc = sandbox.ejecutar("--abi", "x86_64")
        _exigir(
            proc.returncode == 0,
            "el script fallo con --abi x86_64:\n%s" % (proc.stdout + proc.stderr),
        )
        log = sandbox.invocaciones()
        _exigir(
            "android_x86_64/bin/qt-cmake" in log,
            "no se usa el kit Qt android_x86_64:\n%s" % log,
        )
        _exigir(
            "-DANDROID_ABI=x86_64" in log,
            "no se pasa -DANDROID_ABI=x86_64:\n%s" % log,
        )
        _exigir(
            sandbox.apk("x86_64").is_file(),
            "no se genera apks/NovaStudio-x86_64.apk",
        )
        staging = sandbox.staging_gradle()
        _exigir(
            sandbox.lineas_de(staging, "abiFilters") == ["abiFilters 'x86_64'"],
            "el staging no queda con un unico abiFilters 'x86_64':\n%s"
            % staging.read_text(),
        )
        do_not_strip = sandbox.lineas_de(staging, "doNotStrip")
        _exigir(
            bool(do_not_strip)
            and all("*/x86_64/*.so" in linea for linea in do_not_strip),
            "el staging no queda con doNotStrip para x86_64:\n%s" % staging.read_text(),
        )


COMPROBACIONES = (
    ("soporta_parametro_abi", soporta_parametro_abi),
    ("abi_por_defecto_arm64", abi_por_defecto_arm64),
    ("toolchain_derivado_del_abi", toolchain_derivado_del_abi),
    ("nombre_de_apk_incluye_abi", nombre_de_apk_incluye_abi),
    ("pasa_android_abi_a_cmake", pasa_android_abi_a_cmake),
    ("ajusta_abifilters_del_staging", ajusta_abifilters_del_staging),
    ("rechaza_abi_invalido", lambda _texto: rechaza_abi_invalido()),
    ("genera_apk_con_el_abi_elegido", lambda _texto: genera_apk_con_el_abi_elegido()),
)


def main() -> int:
    texto = SCRIPT.read_text()

    print("== Contrato de build_android.sh ==")
    print(f"   {len(COMPROBACIONES)} comprobaciones\n")

    fallos = 0
    for nombre, comprobacion in COMPROBACIONES:
        try:
            comprobacion(texto)
        except AssertionError as error:
            fallos += 1
            print(f"   [FALLO] {nombre}")
            for linea in str(error).splitlines():
                print(f"            {linea}")
        else:
            print(f"   [OK]    {nombre}")

    print("")
    if fallos:
        print(f"   !! {fallos} comprobacion(es) fallan: el script no cumple el contrato de ABI.")
        return 1

    print("   OK: build_android.sh compila para el ABI que se le pida.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
