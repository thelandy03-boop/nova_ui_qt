#!/bin/bash
# Publica una nueva release de NOVA Studio en GitHub con el APK adjunto.
# El sistema de actualizacion obligatoria (NovaUpdateChecker) consulta
# siempre la release marcada como "latest".
#
# Uso:
#   ./publish_release.sh [patch|minor|major] [--with-x86]
#
#   patch|minor|major  Tipo de incremento de version (por defecto: patch).
#   --with-x86         Adjunta tambien el APK x86_64 debug (Waydroid/emulador).
#                      OJO: no se adjunta por defecto porque el checker elige
#                      el primer asset .apk y ese no sirve en telefonos ARM.
set -e

BUMP="patch"
WITH_X86=0
for arg in "$@"; do
    case "$arg" in
        patch|minor|major) BUMP="$arg" ;;
        --with-x86) WITH_X86=1 ;;
        -h|--help)
            echo "Uso: $0 [patch|minor|major] [--with-x86]"
            exit 0 ;;
        *) echo "❌ Opcion desconocida: $arg"; exit 1 ;;
    esac
done

cd "$(dirname "$0")"

# --- Requisitos ---------------------------------------------------------
if ! command -v gh >/dev/null 2>&1; then
    echo "❌ 'gh' no esta instalado. Instalalo con: sudo apt install gh -y"
    exit 1
fi
if ! gh auth status >/dev/null 2>&1; then
    echo "❌ 'gh' no esta autenticado. Ejecuta: gh auth login"
    exit 1
fi
if ! git diff --quiet; then
    echo "❌ Hay cambios sin commitear. Haz commit o stash antes de publicar."
    git status --short
    exit 1
fi

git fetch --tags --quiet 2>/dev/null || true

# --- 1. Calcular nueva version ------------------------------------------
LATEST_TAG=$(git describe --tags --abbrev=0 2>/dev/null || echo "v1.0.0")
VERSION_NUM="${LATEST_TAG#v}"
IFS='.' read -r MAJOR MINOR PATCH <<< "$VERSION_NUM"
MAJOR=${MAJOR:-1}; MINOR=${MINOR:-0}; PATCH=${PATCH:-0}

case "$BUMP" in
    major) MAJOR=$((MAJOR + 1)); MINOR=0; PATCH=0 ;;
    minor) MINOR=$((MINOR + 1)); PATCH=0 ;;
    patch) PATCH=$((PATCH + 1)) ;;
esac

NEW_VERSION="${MAJOR}.${MINOR}.${PATCH}"
NEW_TAG="v${NEW_VERSION}"
VERSION_CODE=$((MAJOR * 10000 + MINOR * 100 + PATCH))

if git rev-parse -q --verify "refs/tags/${NEW_TAG}" >/dev/null; then
    echo "❌ El tag ${NEW_TAG} ya existe. Elige otro incremento."
    exit 1
fi

echo "🚀 Nueva version: ${NEW_TAG} (anterior: ${LATEST_TAG}, versionCode: ${VERSION_CODE})"

# --- 2. Sellar version en el codigo -------------------------------------
sed -i "s/m_currentVersion = QStringLiteral(\"[0-9.]*\")/m_currentVersion = QStringLiteral(\"${NEW_VERSION}\")/" \
    src/core/NovaUpdateChecker.h
sed -i "s/versionName \"[0-9.]*\"/versionName \"${NEW_VERSION}\"/" \
    android/app/build.gradle
sed -i "s/versionCode [0-9]*/versionCode ${VERSION_CODE}/" \
    android/app/build.gradle

# --- 3. Compilar APKs ----------------------------------------------------
echo "📦 Compilando APK arm64-v8a (telefonos)..."
./build_apk.sh --abi arm64-v8a --release
APK_ARM="apks/NovaStudio-arm64-v8a.apk"

FILES=("$APK_ARM")
if [ "$WITH_X86" = "1" ]; then
    echo "📦 Compilando APK x86_64 debug (Waydroid)..."
    ./build_apk.sh --abi x86_64 --debug
    FILES+=("apks/NovaStudio-x86_64-debug.apk")
fi

# --- 4. Commit + tag + push ---------------------------------------------
git add src/core/NovaUpdateChecker.h android/app/build.gradle
git commit -m "chore: bump version to ${NEW_VERSION}"
git tag -a "${NEW_TAG}" -m "Release ${NEW_TAG}"
git push origin main
git push origin "${NEW_TAG}"

# --- 5. Notas de la release ---------------------------------------------
if git rev-parse -q --verify "refs/tags/${LATEST_TAG}" >/dev/null; then
    NOTES=$(git log --pretty=format:"- %s" "${LATEST_TAG}..${NEW_TAG}" | head -n 40)
else
    NOTES=$(git log --pretty=format:"- %s" -n 20)
fi

# --- 6. Publicar en GitHub ----------------------------------------------
echo "📦 Publicando ${NEW_TAG} en GitHub Releases..."
gh release create "${NEW_TAG}" "${FILES[@]}" \
    --title "NOVA Studio ${NEW_TAG}" \
    --notes "Release ${NEW_TAG}

${NOTES}

**Instalacion:** descarga el APK y permita instalar apps de origen desconocido." \
    --latest

echo ""
echo "🎉 ========================================================"
echo "✅ RELEASE ${NEW_TAG} PUBLICADO"
echo "   APK(s): ${FILES[*]}"
echo "   https://github.com/thelandy03-boop/nova_ui_qt/releases/tag/${NEW_TAG}"
echo "==========================================================="
