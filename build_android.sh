#!/bin/bash
set -e

# 0. Parámetros
#    --abi <arm64-v8a|x86_64>: ABI del APK. Por defecto arm64-v8a (teléfonos
#    reales); x86_64 es lo que necesita la imagen de Waydroid. Un APK solo
#    instala si el ABI que lleva dentro coincide con el del dispositivo.
#    --debug: APK Debug (en vez de Release). El nombre del APK lleva el
#    sufijo "-debug" para no pisar el de Release.
ABI="${ABI:-arm64-v8a}"
BUILD_TYPE="release"

while [ $# -gt 0 ]; do
    case "$1" in
        --abi)
            if [ -z "${2:-}" ]; then
                echo "❌ Error: --abi necesita un valor (arm64-v8a o x86_64)"
                exit 1
            fi
            ABI="$2"
            shift 2
            ;;
        --debug)
            BUILD_TYPE="debug"
            shift
            ;;
        -h|--help)
            echo "Uso: $(basename "$0") [--abi <arm64-v8a|x86_64>] [--debug]"
            exit 0
            ;;
        *)
            echo "❌ Error: opción desconocida '$1'"
            echo "   Uso: $(basename "$0") [--abi <arm64-v8a|x86_64>] [--debug]"
            exit 1
            ;;
    esac
done

case "${ABI}" in
    arm64-v8a|x86_64) ;;
    *)
        echo "❌ Error: ABI '${ABI}' no soportado."
        echo "   ABIs admitidos: arm64-v8a (teléfonos), x86_64 (Waydroid/emulador)."
        exit 1
        ;;
esac

echo "🚀 [NOVA ANDROID BUILD] Configurando entorno de compilación..."

# 1. Variables del sistema Android y Java 17 LTS (Oficial para AGP 8 / Qt 6.8)
export ANDROID_SDK_ROOT="$HOME/Android/Sdk"
export ANDROID_NDK_ROOT="$HOME/Android/Sdk/ndk/26.1.10909125"

if [ -d "/usr/lib/jvm/java-17-openjdk-amd64" ]; then
    export JAVA_HOME="/usr/lib/jvm/java-17-openjdk-amd64"
elif [ -d "/usr/lib/jvm/java-21-openjdk-amd64" ]; then
    export JAVA_HOME="/usr/lib/jvm/java-21-openjdk-amd64"
else
    export JAVA_HOME="/usr/lib/jvm/default-java"
fi

export PATH="${JAVA_HOME}/bin:${PATH}"

echo "  - SDK Root:  ${ANDROID_SDK_ROOT}"
echo "  - NDK Root:  ${ANDROID_NDK_ROOT}"
echo "  - ABI:       ${ABI}"
echo "  - Tipo:      ${BUILD_TYPE}"
echo "  - Java Home: ${JAVA_HOME} ($(${JAVA_HOME}/bin/java -version 2>&1 | head -n 1))"

# 2. Herramienta Qt CMake para Android. El kit se llama android_<abi> con
#    guiones bajos (arm64-v8a -> android_arm64_v8a, x86_64 -> android_x86_64),
#    así que el directorio se deriva del ABI, no al revés.
QT_ABI_DIR="${ABI//-/_}"
QT_ANDROID_DIR="$HOME/Qt/6.8.3/android_${QT_ABI_DIR}"
QT_ANDROID_CMAKE="${QT_ANDROID_DIR}/bin/qt-cmake"

if [ ! -f "$QT_ANDROID_CMAKE" ]; then
    echo "❌ Error: No se encontró qt-cmake en $QT_ANDROID_CMAKE"
    exit 1
fi

BUILD_DIR="build_android"

# El cache de CMake fija el ABI (CMAKE_ANDROID_ARCH_ABI) y no se puede cambiar
# en caliente: si el directorio es de otro ABI, se regenera desde cero.
if [ -f "${BUILD_DIR}/CMakeCache.txt" ] \
    && ! grep -q "^ANDROID_ABI[^=]*=${ABI}\$" "${BUILD_DIR}/CMakeCache.txt"; then
    echo "  ♻️ ${BUILD_DIR} era de otro ABI: se regenera"
    rm -rf "${BUILD_DIR}"
fi

echo "🔨 [NOVA ANDROID BUILD] Generando archivos CMake en ./${BUILD_DIR}..."
${QT_ANDROID_CMAKE} -B ${BUILD_DIR} \
    -DANDROID_ABI="${ABI}" \
    -DANDROID_PLATFORM="android-31" \
    -DCMAKE_BUILD_TYPE=Release

echo "📦 [NOVA ANDROID BUILD] Compilando código nativo..."
# Nota: el target `all` incluye el empaquetado APK (falla en Gradle hasta el
# parche posterior), así que se tolera el error aquí; lo nativo sí compila.
cmake --build ${BUILD_DIR} -j$(nproc) || true

echo "📦 [NOVA ANDROID BUILD] Empaquetando con androiddeployqt (se tolera el fallo de Gradle)..."
cmake --build ${BUILD_DIR} --target apk -j$(nproc) || true

# androiddeployqt regenera android-build en cada corrida (incl. un flag que AGP 8.2
# rechaza), así que los parches van DESPUÉS del staging y antes de Gradle manual.
if [ -d "${BUILD_DIR}/android-build" ]; then
    echo "android.lint.vital=false" >> ${BUILD_DIR}/android-build/gradle.properties
    echo "android.lint.abortOnError=false" >> ${BUILD_DIR}/android-build/gradle.properties
    echo "android.javaCompile.suppressSourceTargetDeprecationWarning=true" >> ${BUILD_DIR}/android-build/gradle.properties
    # Qt 6.8 escribe un flag que AGP 8.1+ eliminó (default true) y aborta si existe
    sed -i '/android.bundle.enableUncompressedNativeLibs/d' ${BUILD_DIR}/android-build/gradle.properties
    # Asegurar pluginManagement (Gradle 8+ lo exige en settings; Qt no lo genera)
    cp android/settings.gradle ${BUILD_DIR}/android-build/settings.gradle
    # El módulo app necesita su manifest (el template lo guarda en src/main/)
    mkdir -p ${BUILD_DIR}/android-build/app/src/main
    cp android/app/src/main/AndroidManifest.xml ${BUILD_DIR}/android-build/app/src/main/AndroidManifest.xml
    # 🔒 Los jars de Qt (org.qtproject.qt.android.*) tienen que estar en
    # app/libs/ y declarados como dependencia (fileTree). Sin esto el Java de
    # la app no compila: "package org.qtproject.qt.android does not exist".
    if [ -d "${QT_ANDROID_DIR}/jar" ]; then
        mkdir -p ${BUILD_DIR}/android-build/app/libs
        cp -f ${QT_ANDROID_DIR}/jar/*.jar ${BUILD_DIR}/android-build/app/libs/ 2>/dev/null || true
        echo "  - Jars de Qt copiados a app/libs/: $(ls ${BUILD_DIR}/android-build/app/libs/ | wc -l)"
    else
        echo "  ⚠️ No se encontró el directorio de jars de Qt: ${QT_ANDROID_DIR}/jar"
    fi

    # 🔒 El staging arrastra el build.gradle del proyecto, que fija
    # abiFilters 'arm64-v8a', 'armeabi-v7a': el APK se empaquetaría con las
    # libs de otro ABI. Se reescribe en el staging (no en el fuente, que
    # sigue siendo el de arm64) para que solo viaje el ABI elegido.
    STAGING_GRADLE="${BUILD_DIR}/android-build/app/build.gradle"
    if [ -f "${STAGING_GRADLE}" ]; then
        sed -i "s|abiFilters.*|abiFilters '${ABI}'|" "${STAGING_GRADLE}"
        # Las .so nativas del ABI nuevo tampoco se deben comprimir ni stripear.
        sed -i "s|doNotStrip .*|doNotStrip \"*/${ABI}/*.so\"|" "${STAGING_GRADLE}"
    else
        echo "  ⚠️ No se encontró el build.gradle del staging: no se ajustan los ABI"
    fi
fi

echo "📦 [NOVA ANDROID BUILD] Gradle manual (assemble${BUILD_TYPE^})..."
# OJO: el staging de Gradle lo genera el target `apk` de Qt (androiddeployqt).
# El target `apk` tambien intenta el build de Gradle y falla hasta el parche
# posterior, por eso su error se tolera arriba y aqui se hace el build de verdad.
# El APK Debug sale de la tarea `assembleDebug` de Gradle (app debuggable, key de
# depuracion, jniDebuggable). Las .so nativas son las mismas que en Release:
# el codigo nativo se compila con CMAKE_BUILD_TYPE=Release en ambos casos.
GRADLE_TASK="assemble$(printf '%s' "${BUILD_TYPE}" | sed 's/^\(.\)/\U\1/')"
(cd ${BUILD_DIR}/android-build && chmod +x gradlew && ./gradlew "${GRADLE_TASK}")

# 3. Exportación a carpeta visible 'apks/' en la raíz
EXPORT_DIR="apks"
mkdir -p "${EXPORT_DIR}"

if [ "${BUILD_TYPE}" = "debug" ]; then
    APK_SRC="${BUILD_DIR}/android-build/app/build/outputs/apk/debug/app-debug.apk"
    APK_DEST="${EXPORT_DIR}/NovaStudio-${ABI}-debug.apk"
else
    APK_SRC="${BUILD_DIR}/android-build/app/build/outputs/apk/release/app-release.apk"
    APK_DEST="${EXPORT_DIR}/NovaStudio-${ABI}.apk"
fi

if [ -f "${APK_SRC}" ]; then
    cp "${APK_SRC}" "${APK_DEST}"
    echo "  ✅ APK copiado a ${APK_DEST} ($(du -h "${APK_DEST}" | cut -f1))"
else
    echo "  ❌ No se encontró el APK en ${APK_SRC}"
fi

echo ""
echo "🎉 ========================================================"
echo "✅ [ÉXITO] ¡APK ${BUILD_TYPE} de NOVA Studio listo para instalar!"
echo "📂 Ubicación directa:"
echo "   👉 $(pwd)/${APK_DEST}"
echo "==========================================================="