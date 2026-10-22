#!/usr/bin/env python3
"""Comprueba las invariantes del camino de build de Android.

Este repo tiene dos etapas distintas y es facil confundirlas:

  CMake (qt-cmake)      -> compila TODO el C++, incluido el motor de audio
  androiddeployqt       -> genera el staging de Gradle
  Gradle                -> solo empaqueta el .so en un APK

Los errores mas caros de este repo han venido de saltarse esa separacion:
anadir un bloque externalNativeBuild al build.gradle de Gradle hace que
Gradle invoque un CMake que no existe, y ese CMake falla con "Could not find
a package configuration file provided by oboe" (Oboe llega por FetchContent:
no existe ningun oboeConfig.cmake en el sistema). Una hora perdida persiguiendo
un error del "motor de audio" que en realidad era una llamada a CMake
imposible.

Este test fija las invariantes que, si se rompen, hacen que el build de
Android deje de funcionar. Es la red de seguridad para el agente (y para
cualquiera) que trabaje en este repo sin conocer el pipeline.

Uso: python3 tests/check_android_pipeline.py
"""
import os
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent

CMAKE_LISTS = ROOT / "CMakeLists.txt"
BUILD_GRADLE = ROOT / "android" / "app" / "build.gradle"
BUILD_SCRIPT = ROOT / "build_android.sh"
AGENTS = ROOT / "AGENTS.md"
VERIFY = ROOT / "scripts" / "verify.sh"
CONTRATOS = ROOT / "scripts" / "verify_contracts.sh"

# El CMakeLists obsoleto que quedo en el arbol: apuntaba a Qt 6.6 y a un
# find_package(oboe) imposible. Se elimino; si vuelve a aparecer, la trampa
# vuelve con el.
CMAKE_OBSOLETO = ROOT / "android" / "app" / "src" / "main" / "cpp" / "CMakeLists.txt"

# La parte Java: lo que decide si al abrir la app el usuario se queda en ella
# o se la sacan (a los Ajustes del sistema, o con un crash).
ACTIVITY = ROOT / "android" / "app" / "src" / "main" / "java" / "com" / "novastudio" / "nova" / "MainActivity.java"
SERVICIO = ROOT / "android" / "app" / "src" / "main" / "java" / "com" / "novastudio" / "nova" / "AudioForegroundService.java"

# El manifest que usa de verdad AGP: el del modulo app. El de la raiz
# (android/AndroidManifest.xml) lo parchea androiddeployqt, pero AGP lo
# ignora: es codigo muerto. Por eso las comprobaciones de arranque leen
# ESTE manifest y no el otro.
MANIFEST_APP = ROOT / "android" / "app" / "src" / "main" / "AndroidManifest.xml"

# La copia muerta de la plantilla: se copiaba al staging y no la leia nadie.
MANIFEST_DUPLICADO = ROOT / "android" / "src" / "main"


def _exigir(condicion: bool, mensaje: str) -> None:
    if not condicion:
        raise AssertionError(mensaje)


def _leer(ruta: pathlib.Path) -> str:
    return ruta.read_text(encoding="utf-8", errors="replace")


def _codigo(texto: str) -> str:
    """El fichero sin comentarios: el contrato habla de codigo, no de prosa.

    Se quitan los comentarios de una linea de los dos estilos del repo: `#`
    (CMake y shell) y `//` (los ficheros de Gradle).
    """
    return "\n".join(
        linea
        for linea in texto.splitlines()
        if not linea.lstrip().startswith(("#", "//"))
    )


# ---------------------------------------------------------------------------
# 1. Oboe se trae con FetchContent (y no con find_package, que no puede
#    funcionar: no hay oboeConfig.cmake en ningun sitio).
# ---------------------------------------------------------------------------


def oboe_viene_de_fetchcontent() -> None:
    codigo = _codigo(_leer(CMAKE_LISTS))
    _exigir(
        "FetchContent_Declare" in codigo and "oboe" in codigo,
        "el CMakeLists raiz ya no trae Oboe con FetchContent",
    )
    _exigir(
        re.search(r"FetchContent_MakeAvailable\s*\(\s*oboe\s*\)", codigo) is not None,
        "el CMakeLists raiz no hace FetchContent_MakeAvailable(oboe)",
    )
    _exigir(
        "find_package(oboe" not in codigo.replace(" ", ""),
        "el CMakeLists raiz usa find_package(oboe): no existe ningun "
        "oboeConfig.cmake, esa llamada falla siempre",
    )


def el_target_de_oboe_es_el_correcto() -> None:
    codigo = _codigo(_leer(CMAKE_LISTS))
    _exigir(
        re.search(r"target_link_libraries\s*\([^)]*\boboe\b", codigo, re.S) is not None,
        "el CMakeLists raiz no enlaza el target 'oboe'",
    )


# ---------------------------------------------------------------------------
# 2. Gradle no compila el nativo: el .so lo deja androiddeployqt.
# ---------------------------------------------------------------------------


def gradle_no_compila_el_codigo_nativo() -> None:
    texto = _leer(BUILD_GRADLE)
    codigo = _codigo(texto)
    _exigir(
        "externalNativeBuild" not in codigo,
        "android/app/build.gradle ha ganado un bloque externalNativeBuild: "
        "Gradle pasaria a compilar el nativo con un CMake que no existe",
    )
    _exigir(
        "jniLibs.srcDirs" in codigo,
        "android/app/build.gradle ya no declara los jniLibs: el APK saldria "
        "sin las .so nativas",
    )
    _exigir(
        "fileTree(dir: 'libs'" in codigo or 'fileTree(dir: "libs"' in codigo,
        "android/app/build.gradle ya no declara los jars de Qt: el codigo Java "
        "de la app no compila",
    )


def el_cmake_obsoleto_no_existe() -> None:
    _exigir(
        not CMAKE_OBSOLETO.exists(),
        "vuelve el CMakeLists obsoleto de android/app/src/main/cpp/: apunta a "
        "Qt 6.6 y a un find_package(oboe) imposible. Es una trampa para "
        "cualquiera que intente compilar desde ahi",
    )


def los_assets_qml_se_empaquetan() -> None:
    """El APK debe llevar los modulos QML (el bundle RCC con los qmldir).

    androiddeployqt deja el bundle en <proyecto>/assets. Si el build.gradle
    del modulo app no declara assets.srcDirs, Gradle no lo empaqueta: el APK
    sale sin el metadata de los modulos QML, "import QtQuick.Controls" no se
    puede resolver en el movil, el QML no carga y la app se cierra al abrirla
    (main() devuelve -1).
    """
    texto = _leer(BUILD_GRADLE)
    _exigir(
        "assets.srcDirs" in texto,
        "android/app/build.gradle no declara assets.srcDirs: el APK saldra sin "
        "los modulos QML (el bundle RCC) y en el movil el QML no cargara",
    )
    _exigir(
        "../assets" in texto,
        "android/app/build.gradle no apunta a los assets que deja "
        "androiddeployqt (../assets desde el modulo app)",
    )


def la_app_no_salta_a_los_ajustes_al_arrancar() -> None:
    """Abrir la app no debe sacar al usuario de ella.

    La MainActivity pedia MANAGE_EXTERNAL_STORAGE al arrancar y abria la
    pantalla de Ajustes del SISTEMA: la app desaparecia de golpe. Y ademas
    pedia RECORD_AUDIO antes de que el usuario concediera nada, lo que en
    Android 14+ hace que el servicio en primer plano de tipo "microphone"
    lance SecurityException y mate el proceso.
    """
    actividad = _leer(ACTIVITY)
    _exigir(
        "ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION" not in actividad,
        "MainActivity sigue abriendo la pantalla de Ajustes del sistema para "
        "pedir acceso a todos los archivos: al abrir la app, el usuario sale "
        "de ella de golpe",
    )
    _exigir(
        "requestPermissionsIfNeeded();" not in actividad,
        "MainActivity sigue pidiendo permisos al arrancar: el dialogo de "
        "microfono sale encima de la app y el servicio en primer plano de "
        "tipo microphone revienta en Android 14+",
    )

    servicio = _leer(SERVICIO)
    _exigir(
        "FOREGROUND_SERVICE_TYPE_MICROPHONE" in servicio,
        "AudioForegroundService ya no contempla el tipo microphone: si el "
        "permiso esta concedido, el microfono tiene que poder activarse",
    )
    _exigir(
        "checkSelfPermission" in servicio,
        "AudioForegroundService no comprueba si RECORD_AUDIO esta concedido "
        "antes de pedir el tipo microphone: en Android 14+ eso es un "
        "SecurityException que mata la app",
    )


# ---------------------------------------------------------------------------
# 3. El arranque en Android: los recursos de Qt, el nombre de la libreria
#    nativa y el servicio en primer plano.
#
#    Son tres causas independientes del mismo crash: la app se cerraba al
#    abrirla en el movil. Cada una se shoota por su cuenta, asi que arreglar
#    una no tapa la siguiente.
# ---------------------------------------------------------------------------


def los_recursos_de_qt_estan_en_el_apk() -> None:
    """El APK debe llevar los recursos de las librerias de Qt (libs.xml).

    androiddeployqt escribe <staging>/res/values/libs.xml con los recursos
    que QtLoader lee del APK (use_local_qt_libs, qt_libs, bundled_libs...). El
    modulo app no los empaquetaba: sin res.srcDirs, Gradle solo miraba
    src/main/res, que no tiene el libs.xml (los .jar de Qt tampoco llevan
    recursos). En el movil, QtLoader busca "use_local_qt_libs" con
    getIdentifier(), no lo encuentra (devuelve 0) y getString(0) lanza un
    Resources.NotFoundException al abrir la app: crash.
    """
    texto = _leer(BUILD_GRADLE)
    _exigir(
        "res.srcDirs" in texto,
        "android/app/build.gradle no declara res.srcDirs: el APK saldra sin "
        "los recursos de las librerias de Qt (res/values/libs.xml) y en el "
        "movil la app reventara al abrirla",
    )
    _exigir(
        "'../res'" in texto or '"../res"' in texto,
        "android/app/build.gradle no apunta a los recursos que deja "
        "androiddeployqt (../res desde el modulo app)",
    )
    _exigir(
        "'src/main/res'" in texto or '"src/main/res"' in texto,
        "android/app/build.gradle no apunta a los recursos propios del modulo "
        "app (src/main/res): al declarar res.srcDirs, Gradle deja de mirar el "
        "sitio por defecto y los recursos de la app se perderian",
    )


def el_manifest_efectivo_declara_el_nombre_de_la_lib() -> None:
    """El manifest que usa AGP tiene que decir como se llama la libreria.

    QtLoader lee el meta-data "android.app.lib_name" del manifest de la
    activity y le anade el ABI del dispositivo: en un telefono de 64 bits
    carga libnova_ui_qt_arm64-v8a.so. El valor va SIN el sufijo del ABI (Qt
    lo pone). Sin el meta-data, Qt dice "Loading main library failed" y la
    app se cierra al abrirla.
    """
    manifest = _leer(MANIFEST_APP)

    inicio = manifest.find("<activity")
    fin = manifest.find("</activity>", inicio)
    _exigir(inicio != -1 and fin != -1, "el manifest efectivo no declara <activity>")
    activity = manifest[inicio:fin]

    _exigir(
        "android.app.lib_name" in activity,
        "el meta-data android.app.lib_name tiene que estar dentro del "
        "<activity> del manifest efectivo: QtLoader lo lee con getMetaData() "
        "de la activity",
    )

    elementos = [
        elemento
        for elemento in re.findall(r"<meta-data\b[^>]*>", activity)
        if "android.app.lib_name" in elemento
    ]
    _exigir(bool(elementos), "el meta-data android.app.lib_name no esta declarado")
    valor = re.search(r'android:value\s*=\s*"([^"]*)"', elementos[0])
    _exigir(valor is not None, "el meta-data android.app.lib_name no tiene android:value")
    _exigir(
        not re.search(r"_(arm64-v8a|armeabi-v7a|x86_64|x86)$", valor.group(1)),
        f"el meta-data android.app.lib_name vale '{valor.group(1)}': el ABI lo "
        "anade Qt, el valor va sin el (la libreria es "
        "libnova_ui_qt_<abi>.so)",
    )


def el_manifest_efectivo_llega_al_staging() -> None:
    """El manifest que usa AGP es el del modulo app, y va al staging.

    androiddeployqt parchea el manifest de la raiz (android/AndroidManifest.xml),
    que AGP ignora: el manifest efectivo es
    android/app/src/main/AndroidManifest.xml, y build_android.sh lo copia al
    staging. Si se deja de copiar, el APK se empaqueta con otro manifest (sin
    el nombre de la libreria) y la app no arranca.
    """
    codigo = _codigo(_leer(BUILD_SCRIPT))
    _exigir(
        "android/app/src/main/AndroidManifest.xml" in codigo,
        "build_android.sh ya no copia el manifest efectivo al staging: el APK "
        "se empaquetara con otro manifest y la app no arrancara",
    )


def el_servicio_en_primer_plano_no_mata_la_app() -> None:
    """El tipo del servicio en primer plano tiene que poder usarse de verdad.

    En Android 14+ (targetSdk 35) llamar a startForeground() con un tipo cuyo
    permiso no esta declarado es un SecurityException que mata el proceso. El
    servicio arranca con el tipo "mediaPlayback", asi que el permiso
    FOREGROUND_SERVICE_MEDIA_PLAYBACK tiene que estar en el manifest.

    Y al reves: el tipo "microphone" exige RECORD_AUDIO concedido y no se
    pide al abrir la app, asi que el manifest declara solo "mediaPlayback".
    El tipo y su permiso van y vienen juntos (el dia que haya grabador).
    """
    manifest = _leer(MANIFEST_APP)

    permisos = re.findall(
        r'<uses-permission\s+android:name\s*=\s*"([^"]+)"', manifest
    )
    tipos = re.findall(r'android:foregroundServiceType\s*=\s*"([^"]+)"', manifest)

    _exigir(
        "android.permission.FOREGROUND_SERVICE_MEDIA_PLAYBACK" in permisos,
        "el manifest efectivo no declara FOREGROUND_SERVICE_MEDIA_PLAYBACK: "
        "startForeground() con el tipo mediaPlayback es un SecurityException "
        "en Android 14+ y mata la app",
    )
    _exigir(
        bool(tipos),
        "el manifest efectivo no declara foregroundServiceType en el servicio",
    )

    for tipo in tipos:
        _exigir(
            "microphone" not in tipo,
            f"el servicio declara el tipo '{tipo}': el tipo microphone exige "
            "RECORD_AUDIO concedido (y no se pide al abrir la app), asi que "
            "reventaria con un SecurityException",
        )
        if "microphone" in tipo:
            _exigir(
                "android.permission.FOREGROUND_SERVICE_MICROPHONE" in permisos,
                f"el servicio declara el tipo '{tipo}' pero falta el permiso "
                "FOREGROUND_SERVICE_MICROPHONE: Android 14+ lo exige",
            )

    if "android.permission.FOREGROUND_SERVICE_MICROPHONE" in permisos:
        _exigir(
            any("microphone" in tipo for tipo in tipos),
            "el manifest declara FOREGROUND_SERVICE_MICROPHONE pero el servicio "
            "no declara el tipo microphone: la app anuncia una capacidad de "
            "microfono que no usa",
        )

    # El codigo tiene que mirar lo que declara el manifest antes de pedir el
    # tipo microphone: startForeground() lanza IllegalArgumentException si el
    # tipo pedido no es un subconjunto del declarado.
    servicio = _leer(SERVICIO)
    _exigir(
        "getForegroundServiceType" in servicio,
        "AudioForegroundService pide el tipo microphone sin mirar lo que "
        "declara el manifest: startForeground() lanza IllegalArgumentException "
        "si el tipo pedido no esta en el manifest",
    )


def no_vuelve_el_manifest_duplicado() -> None:
    """android/src/main/ era una copia muerta de la plantilla.

    AGP solo usa el manifest del modulo app (android/app/src/main/). El otro
    (android/src/main/AndroidManifest.xml) se copiaba al staging y no lo leia
    nadie: era una trampa (editar el manifest equivocado creyendo que es el
    bueno) y ademas referenciaba un recurso (@mipmap/ic_launcher_round) que no
    existe.
    """
    _exigir(
        not MANIFEST_DUPLICADO.exists(),
        "vuelve android/src/main/: es una copia muerta de la plantilla que "
        "AGP no lee (el manifest efectivo es el de android/app/src/main/)",
    )


# ---------------------------------------------------------------------------
# 4. El script de build es el unico camino valido y sabe hacer APK Debug.
# ---------------------------------------------------------------------------


def el_script_de_build_sabe_hacer_debug() -> None:
    codigo = _codigo(_leer(BUILD_SCRIPT))
    _exigir("--debug" in codigo, "build_android.sh no admite el parametro --debug")
    _exigir(
        "assembleDebug" in codigo or "BUILD_TYPE" in codigo,
        "build_android.sh no construye la tarea de Gradle segun el tipo",
    )


def el_script_de_build_pasa_el_qt_de_escritorio() -> None:
    """Qt exige QT_HOST_PATH para compilar contra un Qt cross-compilado.

    Sin el, find_package(Qt6) falla con "To use a cross-compiled Qt, please
    set the QT_HOST_PATH cache variable to the location of your host Qt
    installation". El Qt de escritorio va junto al kit de Android.
    """
    codigo = _codigo(_leer(BUILD_SCRIPT))
    _exigir(
        "QT_HOST_PATH" in codigo,
        "build_android.sh no pasa QT_HOST_PATH: Qt exige el Qt de escritorio "
        "para compilar contra un Qt cross-compilado",
    )
    _exigir(
        "gcc_64" in codigo,
        "build_android.sh no busca el Qt de escritorio (gcc_64) junto al kit "
        "de Android",
    )


def el_arch_de_aqt_es_por_abi() -> None:
    """El nombre del kit de Qt para Android es android_<abi>, no linux_gcc_64."""
    fuentes = [_leer(BUILD_SCRIPT)]
    if AGENTS.exists():
        fuentes.append(_leer(AGENTS))
    texto = "\n".join(fuentes)

    _exigir(
        "android_${" in texto or "android_arm64_v8a" in texto,
        "nadie explica que el kit de Qt para Android va en "
        "$HOME/Qt/6.8.3/android_<abi>",
    )

    # La forma equivocada (linux_gcc_64 para Android) da 404 en aqtinstall.
    # Puede aparecer, pero solo como aviso: toda linea que la mencione tiene
    # que decir de alguna forma que esa forma falla.
    marcadores_de_fallo = ("falla", "404", "no funciona", "da error", "imposible")
    for linea in texto.splitlines():
        if "aqt install-qt linux android" not in linea:
            continue
        if "linux_gcc_64" not in linea:
            continue
        _exigir(
            any(marcador in linea.lower() for marcador in marcadores_de_fallo),
            "se documenta como receta un comando de aqtinstall que da 404: "
            f"'{linea.strip()}'. El arch de Android es android_<abi>",
        )


# ---------------------------------------------------------------------------
# 4. El agente tiene la guide y un comando unico de verificacion.
# ---------------------------------------------------------------------------


def el_agente_tiene_la_guide() -> None:
    _exigir(AGENTS.is_file(), "no existe AGENTS.md: el agente no tiene guia de este repo")
    texto = _leer(AGENTS)
    _exigir(
        "build_android.sh" in texto,
        "AGENTS.md no menciona el comando canonico de build de Android",
    )
    _exigir(
        "find_package(oboe" in texto,
        "AGENTS.md no advierte de que find_package(oboe) no puede funcionar",
    )


def hay_un_comando_unico_de_verificacion() -> None:
    _exigir(VERIFY.is_file(), "no existe scripts/verify.sh")
    _exigir(
        os.access(VERIFY, os.X_OK),
        "scripts/verify.sh no es ejecutable",
    )
    texto = _leer(VERIFY)
    _exigir("ctest" in texto, "scripts/verify.sh no corre ctest")
    _exigir(
        "verify_contracts.sh" in texto,
        "scripts/verify.sh no delega en scripts/verify_contracts.sh",
    )

    _exigir(CONTRATOS.is_file(), "no existe scripts/verify_contracts.sh")
    _exigir(
        os.access(CONTRATOS, os.X_OK),
        "scripts/verify_contracts.sh no es ejecutable",
    )
    texto = _leer(CONTRATOS)
    _exigir(
        "check_android_pipeline.py" in texto,
        "scripts/verify_contracts.sh no corre el contrato del pipeline de Android",
    )
    _exigir(
        "ctest" not in texto,
        "scripts/verify_contracts.sh no debe correr ctest: los contratos tienen "
        "que poder correr sin Qt ni el arbol de Ardour (en CI y en la "
        "verificacion post-agente)",
    )


COMPROBACIONES = (
    ("oboe_viene_de_fetchcontent", oboe_viene_de_fetchcontent),
    ("el_target_de_oboe_es_el_correcto", el_target_de_oboe_es_el_correcto),
    ("gradle_no_compila_el_codigo_nativo", gradle_no_compila_el_codigo_nativo),
    ("el_cmake_obsoleto_no_existe", el_cmake_obsoleto_no_existe),
    ("los_assets_qml_se_empaquetan", los_assets_qml_se_empaquetan),
    ("la_app_no_salta_a_los_ajustes_al_arrancar", la_app_no_salta_a_los_ajustes_al_arrancar),
    ("los_recursos_de_qt_estan_en_el_apk", los_recursos_de_qt_estan_en_el_apk),
    ("el_manifest_efectivo_declara_el_nombre_de_la_lib", el_manifest_efectivo_declara_el_nombre_de_la_lib),
    ("el_manifest_efectivo_llega_al_staging", el_manifest_efectivo_llega_al_staging),
    ("el_servicio_en_primer_plano_no_mata_la_app", el_servicio_en_primer_plano_no_mata_la_app),
    ("no_vuelve_el_manifest_duplicado", no_vuelve_el_manifest_duplicado),
    ("el_script_de_build_sabe_hacer_debug", el_script_de_build_sabe_hacer_debug),
    ("el_script_de_build_pasa_el_qt_de_escritorio", el_script_de_build_pasa_el_qt_de_escritorio),
    ("el_arch_de_aqt_es_por_abi", el_arch_de_aqt_es_por_abi),
    ("el_agente_tiene_la_guide", el_agente_tiene_la_guide),
    ("hay_un_comando_unico_de_verificacion", hay_un_comando_unico_de_verificacion),
)


def main() -> int:
    print("== Contrato del pipeline de build de Android ==")
    print(f"   {len(COMPROBACIONES)} comprobaciones\n")

    fallos = 0
    for nombre, comprobacion in COMPROBACIONES:
        try:
            comprobacion()
        except AssertionError as error:
            fallos += 1
            print(f"   [FALLO] {nombre}")
            for linea in str(error).splitlines():
                print(f"            {linea}")
        except Exception as error:  # noqa: BLE001 - un error raro tambien es un fallo
            fallos += 1
            print(f"   [FALLO] {nombre}")
            print(f"            error inesperado: {error!r}")
        else:
            print(f"   [OK]    {nombre}")

    print("")
    if fallos:
        print(
            f"   !! {fallos} comprobacion(es) fallan: el pipeline de build de "
            "Android esta roto o a punto de romperse."
        )
        return 1

    print("   OK: el pipeline de build de Android mantiene sus invariantes.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
