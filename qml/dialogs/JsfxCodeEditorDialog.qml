import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../skins/logic/dock"

/**
 * Diálogo flotante modal para el Editor JSFX.
 * Reutiliza el componente modular LogicJsfxDock en un contenedor de ventana o diálogo.
 */
Rectangle {
    id: root

    property alias trackIndex: dock.trackIndex
    property alias pluginIndex: dock.pluginIndex
    property alias pluginName: dock.pluginName
    property alias isNewPlugin: dock.isNewPlugin
    property alias isModified: dock.isModified

    signal closeRequested()
    signal scriptApplied()

    color: "#181A20"
    border.color: "#2D3139"
    border.width: 1

    LogicJsfxDock {
        id: dock
        anchors.fill: parent

        onCloseRequested: root.closeRequested()
        onScriptApplied: root.scriptApplied()
    }

    function openForPlugin(trkIdx, plgIdx, plgNm) {
        dock.openForPlugin(trkIdx, plgIdx, plgNm)
    }

    function openNewScript(trkIdx) {
        dock.openNewScript(trkIdx)
    }
}