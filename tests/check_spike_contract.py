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
  - La forma de onda se pinta en la PRIMERA carga: el controlador avisa
    cuando publica los picos y el QML repinta la onda (la carga es
    asincrona, asi que los picos no existen al pulsar "Reproducir").
  - El hilo de trabajo no toca el puntero del cargador (el cargador se
    puede destruir en el hilo de la interfaz mientras el trabajo sigue).
  - "Parar" durante una carga en vuelo se respeta.
  - Una carga fallida no destruye lo que esta sonando.

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
CONTROLADOR_H = SRC / "core" / "platform" / "NovaSpikeController.h"
CARGADOR = SRC / "audio" / "NovaSpikeLoader.cpp"
CARGADOR_H = SRC / "audio" / "NovaSpikeLoader.h"
PAGINA = ROOT / "qml" / "spike" / "SpikePage.qml"


def leer(path: pathlib.Path) -> str:
    return path.read_text(errors="ignore")


def cuenta(texto: str, aguja: str) -> int:
    return texto.count(aguja)


def main() -> int:
    motor = leer(MOTOR)
    motor_h = leer(MOTOR_H)
    controlador = leer(CONTROLADOR)
    controlador_h = leer(CONTROLADOR_H)
    cargador = leer(CARGADOR)
    cargador_h = leer(CARGADOR_H)
    pagina = leer(PAGINA)

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

    # ── La onda se tiene que pintar en la PRIMERA carga ─────────────────
    # La carga es asincrona: los picos no existen cuando se pulsa
    # "Reproducir", asi que el renderizador (que solo lee la cache cuando
    # se crea) se queda en blanco. El controlador tiene que avisar cuando
    # publica los picos y el QML tiene que repintar la onda.
    comprobar("el_controlador_expone_la_version_de_los_picos",
              re.search(r"Q_PROPERTY\s*\(\s*int\s+versionPicos\s+READ\s+versionPicos\s+"
                        r"NOTIFY\s+versionPicosChanged\s*\)", controlador_h) is not None,
              "el controlador tiene que exponer la version de los picos (una propiedad con su "
              "senal de aviso) para que el QML sepa cuando hay una forma de onda nueva")

    comprobar("el_controlador_avisa_al_publicar_los_picos",
              re.search(r"NovaWaveformCache::setPeaks\([^;]*;[^}]*"
                        r"Q_EMIT\s+versionPicosChanged\s*\(\s*\)", controlador, re.S) is not None,
              "el controlador tiene que avisar (versionPicosChanged) DESPUES de publicar los "
              "picos: si avisa antes, el QML repintaria una onda que todavia no esta en la cache")

    comprobar("el_qml_repinta_la_onda_cuando_llegan_los_picos",
              "onVersionPicosChanged" in pagina
              and re.search(r"onVersionPicosChanged\s*\(\s*\)\s*\{[^}]*cargadorOnda\.active",
                            pagina, re.S) is not None,
              "la pagina QML tiene que recrear el item de la forma de onda cuando el controlador "
              "avisa de que los picos estan listos (el renderizador solo lee la cache al crearse)")

    # El boton "Reproducir" no puede depender de recrear el item en el
    # clic: en ese momento los picos todavia no existen (la carga es
    # asincrona) y la onda se quedaria en blanco hasta el siguiente clic.
    # El codigo del boton va desde su texto hasta el boton siguiente.
    inicio_boton = pagina.find('text: "Reproducir"')
    fin_boton = pagina.find('text: "Parar"')
    codigo_boton = (pagina[inicio_boton:fin_boton]
                    if inicio_boton >= 0 and fin_boton > inicio_boton
                    else "")
    comprobar("el_boton_reproducir_no_depende_de_un_segundo_clic",
              bool(codigo_boton) and "cargadorOnda.active" not in codigo_boton,
              "el boton 'Reproducir' no tiene que recrear el item de la onda: los picos todavia no "
              "existen (se calculan en un hilo aparte), la repintura la dispara el aviso del "
              "controlador")

    # ── El hilo de trabajo no puede tocar el puntero del cargador ───────
    comprobar("el_cargador_no_usa_qpointer_en_el_hilo_de_trabajo",
              "QPointer<" not in cargador and "QPointer<" not in cargador_h,
              "el hilo de trabajo no puede mirar un QPointer del cargador: el cargador se puede "
              "destruir en el hilo de la interfaz mientras el trabajo sigue (carrera y acceso a "
              "memoria liberada)")

    comprobar("la_entrega_del_cargador_esta_protegida",
              "EstadoEntrega" in cargador_h and "std::mutex" in cargador_h
              and "vivo" in cargador_h
              and re.search(r"~NovaSpikeLoader\s*\(\s*\)[^{]*\{[^}]*vivo\.store\(\s*false\s*\)",
                            cargador, re.S) is not None,
              "el destructor del cargador tiene que marcar el estado de entrega como 'no vivo' "
              "bajo un candado: asi el hilo de trabajo o entrega con el cargador vivo, o no "
              "entrega nada")

    # ── "Parar" durante una carga en vuelo ─────────────────────────────
    comprobar("parar_durante_la_carga_se_respeta",
              "m_paradaPedida" in controlador
              and re.search(r"void\s+NovaSpikeController::stop\s*\(\s*\)[^{]*\{[^}]*"
                            r"m_paradaPedida\s*=\s*true", controlador, re.S) is not None
              and re.search(r"void\s+NovaSpikeController::alCargarTerminado\s*\(\s*\)[^{]*\{[^}]*"
                            r"m_paradaPedida", controlador, re.S) is not None,
              "la carga es asincrona: si el usuario pulsa 'Parar' mientras se decodifica, el "
              "resultado tiene que descartarse al terminar (si no, la reproduccion arranca igual)")

    comprobar("el_parar_no_arranca_a_la_carga_siguiente",
              re.search(r"bool\s+NovaSpikeController::loadAndPlay\s*\([^)]*\)\s*\{.*?"
                        r"m_paradaPedida\s*=\s*false", controlador, re.S) is not None,
              "una carga nueva tiene que limpiar la bandera de parada: si no, el 'Parar' que se "
              "puso en la carga anterior haria que esta se descartase al terminar")

    # ── Un fallo no puede destruir lo que esta sonando ─────────────────
    comprobar("un_fallo_no_destruye_lo_que_suena",
              re.search(r"void\s+NovaSpikeController::alCargarFallido\s*\([^)]*\)[^{]*\{[^}]*"
                            r"limpiarOndaSiNoSuena\s*\(\s*\)", controlador, re.S) is not None
              and re.search(r"void\s+NovaSpikeController::limpiarOndaSiNoSuena\s*\(\s*\)[^{]*\{"
                            r"[^}]*hayFicheroEnElMotor\s*\(\s*\)", controlador, re.S) is not None,
              "si una carga falla mientras otro fichero suena, su forma de onda y su duracion se "
              "tienen que quedar (si no, el cabezal desaparece y la posicion avanza contra una "
              "duracion de cero)")

    # La pregunta que decide es "¿hay fichero en el motor?", no "¿esta el
    # transporte en marcha?": el transporte arranca con el motor, aunque no
    # haya ningun fichero cargado (con la segunda pregunta la onda nunca se
    # podria borrar).
    comprobar("la_onda_se_borra_segun_si_hay_fichero_en_el_motor",
              re.search(r"bool\s+NovaSpikeController::hayFicheroEnElMotor\s*\(\s*\)\s*const"
                        r"[^{]*\{[^}]*tieneFicheroCargado\s*\(\s*\)", controlador, re.S) is not None
              and "tieneFicheroCargado" in motor_h
              and "tieneFicheroCargado" in motor,
              "el controlador tiene que preguntar al motor si tiene un fichero cargado (no si el "
              "transporte esta en marcha: el transporte arranca con el motor, sin fichero)")

    # El QML tambien se entera de que la onda se ha borrado: si no, el
    # renderizador sigue dibujando la forma de onda anterior (la cache ya
    # no la tiene, pero el renderizador solo la relee cuando se crea).
    comprobar("el_qml_se_entera_de_una_onda_borrada",
              re.search(r"void\s+NovaSpikeController::limpiarOndaPublicada\s*\(\s*\)[^{]*\{"
                        r"[^}]*Q_EMIT\s+versionPicosChanged\s*\(\s*\)", controlador, re.S) is not None,
              "al borrar la onda publicada hay que avisar tambien: si no, el renderizador sigue "
              "dibujando la forma de onda anterior")

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
