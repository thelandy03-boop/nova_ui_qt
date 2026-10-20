// Página del spike de audio en Android.
//
// Es una página mínima a propósito (esto es un spike, no una pantalla del
// producto): la forma de onda real, el cabezal que se mueve, la ruta del
// fichero, un botón y el estado.
//
// La forma de onda la pinta NovaWaveformItem: pide sus picos a la caché de
// formas de onda con el id "spike", que es el mismo id bajo el que el
// controlador publica los picos del fichero cargado.
import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import NovaStudio 1.0

Window {
    id: pagina
    width: 900
    height: 420
    visible: true
    title: "NOVA Studio - spike de audio"
    color: "#121212"

    // El controlador: tiene el motor de audio, publica los picos y lleva la
    // posición del cabezal.
    SpikeController {
        id: spikeController
    }

    Column {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Text {
            text: "Spike de audio (Android)"
            color: "#e0e0e0"
            font.pixelSize: 18
            font.bold: true
        }

        // ── Onda y cabezal ───────────────────────────────────────────
        Item {
            id: zonaOnda
            width: parent.width
            height: 160

            Rectangle {
                anchors.fill: parent
                color: "#1c1c22"
                border.color: "#33333c"
                border.width: 1
            }

            // La forma de onda real: los picos los publica el controlador
            // bajo el id "spike".
            //
            // Se mete en un Loader porque el renderizador solo vuelve a leer
            // la caché cuando sus picos están vacíos o cuando cambia el
            // regionId: al recrear el item, el renderizador nuevo lee los
            // picos del fichero que se acaba de cargar.
            Loader {
                id: cargadorOnda
                anchors.fill: parent
                sourceComponent: componenteOnda
            }

            Component {
                id: componenteOnda
                NovaWaveformItem {
                    anchors.fill: parent
                    regionId: "spike"
                    startBeat: 0
                    lengthBeats: 4
                    pixelsPerBeat: 80
                    peakColor: "#7aa2f7"
                    rmsColor: "#3b4261"
                }
            }

            // Cabezal: lo coloca la posición real del motor (no un reloj).
            Rectangle {
                id: cabezal
                width: 2
                height: zonaOnda.height
                color: "#f7768e"
                // Sin fichero cargado no hay duración que usar: división por
                // cero evitada.
                visible: spikeController.durationSeconds > 0
                x: (spikeController.durationSeconds > 0)
                   ? (spikeController.positionSeconds / spikeController.durationSeconds) * zonaOnda.width
                   : 0
            }
        }

        // ── Fichero y botones ────────────────────────────────────────
        Row {
            width: parent.width
            spacing: 8

            TextField {
                id: campoRuta
                width: parent.width - 220
                // La carpeta propia de la app en el almacenamiento externo es
                // la unica que se puede leer sin permisos especiales
                // (scoped storage): ahi se copia el fichero de prueba con
                // "adb push /sdcard/Android/data/com.novastudio.nova/files/".
                text: "/sdcard/Android/data/com.novastudio.nova/files/spike.mp3"
                placeholderText: "Ruta del fichero de audio"
                selectByMouse: true
            }

            Button {
                text: "Reproducir"
                onClicked: {
                    spikeController.loadAndPlay(campoRuta.text)
                    // El item de la onda se recrea para que el renderizador
                    // lea los picos del fichero nuevo.
                    cargadorOnda.active = false
                    cargadorOnda.active = true
                }
            }

            Button {
                text: "Parar"
                onClicked: spikeController.stop()
            }
        }

        // ── Estado ───────────────────────────────────────────────────
        Text {
            width: parent.width
            text: spikeController.status
            color: "#c0c0c0"
            font.pixelSize: 14
            elide: Text.ElideRight
        }

        Text {
            width: parent.width
            text: "Cabezal: " + spikeController.positionSeconds.toFixed(2) + " s de "
                  + spikeController.durationSeconds.toFixed(2) + " s"
            color: "#808080"
            font.pixelSize: 12
        }
    }
}
