#!/usr/bin/env python3
"""Comprueba el contrato del camino del spike de audio en Android.

El motor, el controlador y el cargador del spike solo se compilan en Android
(sin el motor de Ardour no existen en el escritorio), asi que sus
comportamientos criticos no se pueden comprobar con un test unitario en el
host. Este test los comprueba leyendo el codigo: es la red de seguridad
para que un cambio no rompa en silencio lo que el revisor ya arreglo.

Lo que comprueba:

  - El motor NO decodifica: recibe el audio ya decodificado (el fichero se
    decodifica una sola vez y el PCM no se copia).
  - El controlador NO decodifica ni calcula picos: eso lo hace el cargador,
    en un hilo aparte (spec 8.4: los picos nunca se calculan en el hilo de
    la interfaz).
  - El motor remuestrea a la tasa REAL del stream (la que da el dispositivo),
    no a la pedida.
  - Hay un log de fotogramas escritos (el criterio objetivo de la
    verificacion en Waydroid).

Uso: python3 tests/check_spike_contract.py
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "src"

MOTOR = SRC / "core" / "platform" / "NovaAndroidAudioEngine.cpp"
MOTOR_H = SRC / "core" / "platform" / "NovaAndroidAudioEngine.h"
CONTROLADOR = SRC / "core" / "platform" / "NovaSpikeController.cpp"
CARGADOR = SRC / "audio" / "NovaSpikeLoader.cpp"


def leer(path: pathlib.Path) -> str:
    return path.read_text(errors="ignore")


def cuenta(texto: str, aguja: str) -> int:
    return texto.count(aguja)


def main() -> int:
    motor = leer(MOTOR)
    motor_h = leer(MOTOR_H)
    controlador = leer(CONTROLADOR)
    cargador = leer(CARGADOR)

    comprobaciones = []

    def comprobar(nombre: str, cond: bool, explicacion: str) -> None:
        comprobaciones.append((nombre, bool(cond), explicacion))

    # ── El fichero se decodifica una sola vez ────────────────────────────
    comprobar("el_motor_no_decodifica",
              cuenta(motor, "decodeAudioFile(") == 0,
              "el motor recibe el audio ya decodificado: no puede volver a decodificar "
              "el fichero (serian dos copias del PCM)")

    comprobar("el_controlador_no_decodifica",
              cuenta(controlador, "decodeAudioFile(") == 0
              and cuenta(controlador, "buildPeakData(") == 0,
              "el controlador no decodifica ni calcula picos: lo hace el cargador, en un hilo aparte")

    comprobar("el_cargador_decodifica_una_sola_vez",
              cuenta(cargador, "decodeAudioFile(") == 1
              and cuenta(cargador, "buildPeakData(") == 1,
              "el cargador decodifica el fichero una sola vez y calcula los picos una sola vez")

    comprobar("el_cargador_trabaja_en_un_hilo_aparte",
              "QThreadPool" in cargador and "Qt::QueuedConnection" in cargador,
              "el cargador tiene que hacer el trabajo pesado en un hilo de trabajo y devolver "
              "el resultado al hilo de la interfaz")

    comprobar("el_controlador_pasa_el_audio_ya_decodificado",
              "loadDecoded(" in controlador and "loadFile(" not in controlador,
              "el controlador tiene que pasar al motor el audio ya decodificado (no la ruta)")

    # ── La tasa real del dispositivo, no la pedida ────────────────────────
    comprobar("el_motor_usa_la_tasa_real_del_stream",
              "getSampleRate()" in motor
              and "m_player.setOutputSampleRate(m_tasaSalidaReal.load())" in motor,
              "tras abrir el stream hay que leer su tasa real (getSampleRate) y aplicarla al "
              "reproductor: si no, el audio suena grave y lento")

    comprobar("el_motor_no_remeuestrea_a_la_tasa_pedida",
              "m_player.setOutputSampleRate(m_sampleRate)" not in motor,
              "el reproductor no se puede poner a la tasa pedida: la que manda es la real "
              "del stream")

    comprobar("la_posicion_usa_la_tasa_real",
              re.search(r"currentTimeSeconds\(\) const\s*\{[^}]*tasaSalidaReal\(\)", motor_h)
              is not None,
              "la posicion (currentTimeSeconds) tiene que medirse con la tasa real del stream")

    # ── El criterio de verificacion tiene que existir en el log ─────────
    comprobar("hay_log_de_fotogramas_escritos",
              "currentFrame()" in controlador and "positionSeconds()" in controlador
              and re.search(r"qCDebug\([^)]*\)[^;]*currentFrame\(\)", controlador) is not None,
              "tiene que haber un log (en el hilo de la interfaz) con los fotogramas escritos y "
              "la posicion: es el criterio objetivo de la verificacion en Waydroid")

    fallos = [c for c in comprobaciones if not c[1]]

    print("== Contrato del spike de Android ==")
    print(f"   {len(comprobaciones)} comprobaciones\n")
    for nombre, ok, explicacion in comprobaciones:
        if ok:
            print(f"   [ok] {nombre}")
        else:
            print(f"   [FALLO] {nombre}: {explicacion}")

    if fallos:
        print(f"\n   {len(fallos)} comprobacion(es) fallan")
        return 1

    print("\n   OK: el camino del spike mantiene el contrato")
    return 0


if __name__ == "__main__":
    sys.exit(main())
