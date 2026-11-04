import QtQuick
import QtQuick.Layouts

/**
 * Minimapa lateral derecho estilo VS Code.
 * Muestra una representación compacta a microescala de las líneas de código
 * con colores temáticos según el tipo de sentencia EEL2 / JSFX.
 */
Rectangle {
    id: root

    property string sourceText: ""
    property int lineCount: 1

    Layout.preferredWidth: 54
    Layout.fillHeight: true
    color: "#181A20"
    clip: true

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: "#22252D"
    }

    Column {
        anchors.fill: parent
        anchors.topMargin: 8
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        spacing: 2
        opacity: 0.65

        Repeater {
            model: Math.min(80, root.lineCount)
            delegate: Item {
                width: 42
                height: 2

                readonly property string lineStr: {
                    var arr = root.sourceText.split("\n")
                    return index < arr.length ? arr[index].trim() : ""
                }

                Rectangle {
                    width: Math.min(parent.width, Math.max(6, parent.lineStr.length * 1.4))
                    height: 2
                    radius: 1
                    color: {
                        if (parent.lineStr.indexOf("//") === 0) return "#628A62"
                        if (parent.lineStr.indexOf("@") === 0) return "#F0C674"
                        if (parent.lineStr.indexOf("slider") === 0) return "#7EC67E"
                        if (parent.lineStr.indexOf("desc:") === 0) return "#E5A44E"
                        return "#7A8294"
                    }
                }
            }
        }
    }
}
