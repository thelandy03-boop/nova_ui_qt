#!/bin/bash
# Compila un APK nuevo de NOVA Studio.
# Uso: ./build_apk.sh [--abi arm64-v8a|x86_64] [--debug|--release]
set -e

ABI="x86_64"
TYPE="debug"
while [ $# -gt 0 ]; do
    case "$1" in
        --abi) ABI="$2"; shift 2 ;;
        --debug) TYPE="debug"; shift ;;
        --release) TYPE="release"; shift ;;
        -h|--help)
            echo "Uso: $0 [--abi arm64-v8a|x86_64] [--debug|--release]"
            exit 0 ;;
        *) echo "Opción desconocida: $1"; exit 1 ;;
    esac
done

echo "🚀 Compilando APK (ABI=$ABI, tipo=$TYPE)..."
./build_android.sh --abi "$ABI" $([ "$TYPE" = "debug" ] && echo "--debug")

APK="apks/NovaStudio-${ABI}$([ "$TYPE" = "debug" ] && echo "-debug").apk"
if [ -f "$APK" ]; then
    echo ""
    echo "✅ APK listo: $(pwd)/$APK ($(du -h "$APK" | cut -f1))"
else
    echo "❌ No se encontró el APK esperado: $APK"
    exit 1
fi
