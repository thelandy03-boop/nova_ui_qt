#!/bin/bash
# Mide el corpus de JSFX reales con el banco de pruebas de YSFX.
#
# Descarga seis repositorios públicos (Saike, Geraint Luff, chkhld, ReJJ,
# JClones y tilr) en un commit fijado, y ejecuta una selección de scripts.
# Cada script se lanza en su propio proceso con un tiempo límite: un script
# roto no puede tumbar la medición entera.
#
# Uso: ./herramientas/medir_corpus.sh
set -euo pipefail

AQUI="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CACHE="${AQUI}/.cache/corpus"
BIN="${AQUI}/build-host/nova_ysfx_spike"
RESULTADOS="${AQUI}/build-host/corpus.tsv"
ERRORES="${AQUI}/build-host/corpus_errores.log"
TIEMPO_LIMITE=25

if [ ! -x "${BIN}" ]; then
    echo "No existe ${BIN}. Ejecuta antes ./compilar_host.sh" >&2
    exit 1
fi

mkdir -p "${CACHE}" "${AQUI}/build-host"
: > "${RESULTADOS}"
: > "${ERRORES}"

# Repositorio -> commit fijado.
declare -A REPOS=(
    ["JoepVanlier/JSFX"]="c1c98b5f5f505238ac4d21ee43f32daf6b885527"
    ["geraintluff/jsfx"]="e98d64401ba57590b7ea0d3592675accb43667da"
    ["chkhld/jsfx"]="f6d175d8253375a51832fd52aa86482c191caadc"
    ["Justin-Johnson/ReJJ"]="956ba4525c45e7b3f704ba8b7940301617deeaaa"
    ["JClones/JSFXClones"]="88a1503d668c378ced4c166e772378272f3b72ea"
    ["tiagolr/tilr_jsfx"]="cd5337533f20aff4d8ce8001e8cf16da94f23e67"
)

# Descarga y extrae cada repositorio una sola vez.
for repo in "${!REPOS[@]}"; do
    destino="${CACHE}/${repo//\//-}"
    if [ -d "${destino}" ]; then
        continue
    fi
    echo "⬇️  ${repo} @ ${REPOS[$repo]:0:8}"
    tarball="${CACHE}/${repo//\//-}.tar.gz"
    curl -fsSL "https://github.com/${repo}/archive/${REPOS[$repo]}.tar.gz" -o "${tarball}"
    mkdir -p "${destino}"
    tar -xzf "${tarball}" --strip-components=1 -C "${destino}"
    rm -f "${tarball}"
done

# Selección representativa: efectos, dinámica, EQ, delays, sintetizadores,
# medidores y algún @gfx. Todos son scripts reales de la comunidad.
SELECCION=(
    "JoepVanlier/JSFX|Basics/Tanh_Saturator_AA.jsfx"
    "JoepVanlier/JSFX|Basics/Tight_Compressor.jsfx"
    "JoepVanlier/JSFX|Basics/Transience.jsfx"
    "JoepVanlier/JSFX|Basics/wahriffic.jsfx"
    "JoepVanlier/JSFX|Basics/MS-20.jsfx"
    "JoepVanlier/JSFX|DuskVerb/saike_duskverb.jsfx"
    "JoepVanlier/JSFX|Nuker/saike_nuker.jsfx"
    "JoepVanlier/JSFX|PhaseMangler/saike_phase_mangler.jsfx"
    "JoepVanlier/JSFX|Neverb/neverb.jsfx"
    "JoepVanlier/JSFX|SpectrumAnalyzer/StereoManipulator.jsfx"

    "geraintluff/jsfx|Smooth Limiter.jsfx"
    "geraintluff/jsfx|Vibrato.jsfx"
    "geraintluff/jsfx|Soft-Bell.jsfx"
    "geraintluff/jsfx|Echo-Cycles.jsfx"
    "geraintluff/jsfx|Level Meter.jsfx"
    "geraintluff/jsfx|MIDI Gate.jsfx"
    "geraintluff/jsfx|Panalysis.jsfx"

    "chkhld/jsfx|Clipper/soft_clipper.jsfx"
    "chkhld/jsfx|Dynamics/bus_comp.jsfx"
    "chkhld/jsfx|Equalizer/eq_560.jsfx"
    "chkhld/jsfx|FX/filthy_delay.jsfx"
    "chkhld/jsfx|Lo-Fi/signal_crusher.jsfx"
    "chkhld/jsfx|Metering/wave_scope.jsfx"

    "Justin-Johnson/ReJJ|ReEQ/ReEQ.jsfx"
    "Justin-Johnson/ReJJ|ReSpectrum/ReSpectrum.jsfx"

    "JClones/JSFXClones|jsfx/JClones_CA2A.jsfx"
    "JClones/JSFXClones|jsfx/JClones_L2.jsfx"
    "JClones/JSFXClones|jsfx/JClones_Phoenix.jsfx"
    "JClones/JSFXClones|jsfx/JClones_SatChannel.jsfx"
    "JClones/JSFXClones|jsfx/JClones_TapeHead.jsfx"

    "tiagolr/tilr_jsfx|Delay/QuickDelay.jsfx"
    "tiagolr/tilr_jsfx|Filter/QuickFilter.jsfx"
    "tiagolr/tilr_jsfx|Synth/js3osc.jsfx"
    "tiagolr/tilr_jsfx|Modulation/adsr1.jsfx"
    "tiagolr/tilr_jsfx|Utility/Interaural Panning.jsfx"
)

echo "▶️  Midiendo ${#SELECCION[@]} scripts"
for entrada in "${SELECCION[@]}"; do
    repo="${entrada%%|*}"
    relativo="${entrada#*|}"
    raiz="${CACHE}/${repo//\//-}"
    archivo="${raiz}/${relativo}"

    if [ ! -f "${archivo}" ]; then
        printf 'FALTA|%s|%s\n' "${entrada}" "${archivo}" | tee -a "${ERRORES}"
        printf 'CORPUS|%s|carga=error|compila=no|audio=no|gfx=no|sliders=0|secciones=-\n' \
            "${entrada}" >> "${RESULTADOS}"
        continue
    fi

    set +e
    salida=$(timeout "${TIEMPO_LIMITE}" "${BIN}" --uno "${archivo}" --raiz-uno "${raiz}" 2>&1)
    estado=$?
    set -e

    linea=$(printf '%s\n' "${salida}" | grep '^CORPUS|' | tail -1 || true)
    if [ -z "${linea}" ]; then
        if [ "${estado}" -eq 124 ]; then
            linea="CORPUS|${entrada}|carga=timeout|compila=?|audio=?|gfx=?|sliders=?|secciones=?"
        else
            linea="CORPUS|${entrada}|carga=crash|compila=?|audio=?|gfx=?|sliders=?|secciones=?"
        fi
        printf '\n=== %s (estado %s) ===\n%s\n' "${entrada}" "${estado}" "${salida}" >> "${ERRORES}"
    elif [ "${estado}" -ne 0 ]; then
        printf '\n=== %s (estado %s) ===\n%s\n' "${entrada}" "${estado}" "${salida}" >> "${ERRORES}"
    fi

    printf '%s\n' "${linea}" | tee -a "${RESULTADOS}"
done

echo
echo "================ RESUMEN ================"
awk -F'|' '
    { total++ }
    /carga=ok/ { carga++ }
    /compila=ok/ { compila++ }
    /audio=ok/ { audio++ }
    /gfx=ok/ { gfx++ }
    /gfx=sin_cambio/ { gfx_no++ }
    END {
        printf "Scripts:          %d\n", total
        printf "Cargan:           %d\n", carga+0
        printf "Compilan:         %d\n", compila+0
        printf "Audio finito:     %d\n", audio+0
        printf "Con @gfx pintado: %d\n", gfx+0
        printf "Con @gfx mudo:    %d\n", gfx_no+0
    }
' "${RESULTADOS}"
echo "Detalle: ${RESULTADOS}"
echo "Errores: ${ERRORES}"
