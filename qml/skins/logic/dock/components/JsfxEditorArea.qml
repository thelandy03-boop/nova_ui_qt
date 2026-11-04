import QtQuick
import QtQuick.Controls

/**
 * Área de edición principal de código EEL2 / JSFX.
 * Incluye el Gutter de números de línea sincronizado matemáticamente,
 * resaltado de línea activa, guías de indentación y cursor pulsante de 2px.
 */
Item {
    id: root

    property alias text: codeTextEdit.text
    property alias cursorPosition: codeTextEdit.cursorPosition
    property alias lineCount: codeTextEdit.lineCount

    property int currentLine: 1
    property int currentCol: 1
    property string currentSection: "@init"

    signal textModified()
    signal cursorMoved()

    function searchInCode(term) {
        if (!term || term.length === 0) return
        var idx = codeTextEdit.text.indexOf(term, codeTextEdit.cursorPosition)
        if (idx === -1) idx = codeTextEdit.text.indexOf(term, 0)
        if (idx !== -1) {
            codeTextEdit.cursorPosition = idx
            codeTextEdit.select(idx, idx + term.length)
        }
    }

    function findNext(term) {
        if (!term || term.length === 0) return
        var start = codeTextEdit.cursorPosition + 1
        var idx = codeTextEdit.text.indexOf(term, start)
        if (idx === -1) idx = codeTextEdit.text.indexOf(term, 0)
        if (idx !== -1) {
            codeTextEdit.cursorPosition = idx
            codeTextEdit.select(idx, idx + term.length)
        }
    }

    function findPrev(term) {
        if (!term || term.length === 0) return
        var start = Math.max(0, codeTextEdit.cursorPosition - 1)
        var idx = codeTextEdit.text.lastIndexOf(term, start)
        if (idx === -1) idx = codeTextEdit.text.lastIndexOf(term)
        if (idx !== -1) {
            codeTextEdit.cursorPosition = idx
            codeTextEdit.select(idx, idx + term.length)
        }
    }

    function forceFocus() {
        codeTextEdit.forceActiveFocus()
    }

    function updateCursorPosition() {
        var pos = codeTextEdit.cursorPosition
        var sub = codeTextEdit.text.substring(0, pos)
        var lines = sub.split("\n")
        root.currentLine = lines.length
        root.currentCol = lines[lines.length - 1].length + 1

        var detectedSection = "desc"
        for (var i = lines.length - 1; i >= 0; --i) {
            var trimmed = lines[i].trim()
            if (trimmed.indexOf("@") === 0) {
                detectedSection = trimmed.split(/\s+/)[0]
                break
            }
        }
        root.currentSection = detectedSection
        root.cursorMoved()
    }

    ScrollView {
        id: editorScrollView
        anchors.fill: parent
        clip: true

        Row {
            id: codeRow
            spacing: 0
            width: Math.max(editorScrollView.width - 60, codeEditorContainer.width + gutter.width + 40)
            height: Math.max(editorScrollView.height, codeTextEdit.contentHeight + 30)

            // GUTTER: NÚMEROS DE LÍNEA EXACTOS PÍXEL POR PÍXEL
            Rectangle {
                id: gutter
                width: 44
                height: parent.height
                color: "#181A20"

                Column {
                    anchors.top: parent.top
                    anchors.topMargin: 8
                    width: parent.width

                    Repeater {
                        model: Math.max(1, codeTextEdit.lineCount)
                        delegate: Item {
                            width: gutter.width
                            height: codeTextEdit.cursorRectangle.height > 0 ? codeTextEdit.cursorRectangle.height : 18

                            readonly property string rawLineText: {
                                var arr = codeTextEdit.text.split("\n")
                                return index < arr.length ? arr[index].trim() : ""
                            }
                            readonly property bool isSection: rawLineText.indexOf("@") === 0

                            // Indicador de línea actual activa
                            Rectangle {
                                anchors.fill: parent
                                color: (index + 1 === root.currentLine) ? "#252832" : "transparent"
                            }

                            // Número de línea alineado a la derecha
                            Text {
                                width: 30
                                anchors.right: parent.right
                                anchors.rightMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                text: (index + 1).toString()
                                color: (index + 1 === root.currentLine) ? "#E6EDF3" : "#555C6E"
                                font.family: codeTextEdit.font.family
                                font.pixelSize: codeTextEdit.font.pixelSize
                                font.bold: (index + 1 === root.currentLine)
                                horizontalAlignment: Text.AlignRight
                            }

                            // Chevron de sección
                            Canvas {
                                visible: parent.isSection
                                width: 7; height: 7
                                anchors.verticalCenter: parent.verticalCenter
                                x: 5
                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.clearRect(0, 0, 7, 7)
                                    ctx.strokeStyle = "#858585"
                                    ctx.lineWidth = 1.2
                                    ctx.beginPath()
                                    ctx.moveTo(1, 2)
                                    ctx.lineTo(3.5, 5)
                                    ctx.lineTo(6, 2)
                                    ctx.stroke()
                                }
                            }
                        }
                    }
                }

                // Línea divisoria vertical del gutter
                Rectangle {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 1
                    color: "#22252D"
                }
            }

            // ZONA EDITABLE DEL CÓDIGO
            Item {
                id: codeEditorContainer
                width: Math.max(editorScrollView.width - gutter.width - 50, codeTextEdit.contentWidth + 40)
                height: parent.height

                // Resaltado de línea activa estilo VS Code
                Rectangle {
                    x: 0
                    y: codeTextEdit.cursorRectangle.y
                    width: parent.width
                    height: codeTextEdit.cursorRectangle.height > 0 ? codeTextEdit.cursorRectangle.height : 18
                    color: "#20232B"
                    border.color: "#282C37"
                    border.width: 1
                    visible: codeTextEdit.activeFocus
                }

                // Guías de indentación verticales
                Row {
                    x: 10 + 32
                    y: 8
                    height: codeTextEdit.contentHeight
                    spacing: 31
                    opacity: 0.12

                    Repeater {
                        model: 6
                        delegate: Rectangle {
                            width: 1
                            height: codeTextEdit.contentHeight
                            color: "#8F94A0"
                        }
                    }
                }

                // TextEdit con renderizado nativo nítido
                TextEdit {
                    id: codeTextEdit
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.topMargin: 8
                    anchors.leftMargin: 10
                    width: parent.width
                    height: Math.max(contentHeight + 30, editorScrollView.height)
                    color: "#D4D4D4"
                    selectedTextColor: "#FFFFFF"
                    selectionColor: "#264F78"
                    font.family: "JetBrains Mono, Consolas, 'Courier New', Monospace"
                    font.pixelSize: 13
                    wrapMode: TextEdit.NoWrap
                    tabStopDistance: 32
                    cursorVisible: true
                    selectByMouse: true

                    // Cursor personalizado de 2px estilo VS Code
                    cursorDelegate: Rectangle {
                        width: 2
                        color: "#AEAFAD"
                        visible: codeTextEdit.activeFocus

                        SequentialAnimation on opacity {
                            loops: Animation.Infinite
                            running: codeTextEdit.activeFocus
                            NumberAnimation { to: 1.0; duration: 100 }
                            PauseAnimation { duration: 400 }
                            NumberAnimation { to: 0.0; duration: 150 }
                            PauseAnimation { duration: 300 }
                        }
                    }

                    onCursorPositionChanged: {
                        root.updateCursorPosition()
                    }

                    onTextChanged: {
                        root.textModified()
                        root.updateCursorPosition()
                    }

                    Component.onCompleted: {
                        root.updateCursorPosition()
                    }
                }
            }
        }
    }
}
