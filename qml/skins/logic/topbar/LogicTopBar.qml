import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"

Rectangle {
    id: root
    implicitHeight: 70
    color: "#2C303A"

    signal collaborationRequested()
    signal homeRequested()

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: "#3F4350"
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: (Qt.platform.os === "android") ? 16 : 4
        anchors.rightMargin: 16
        spacing: 0

        RowLayout {
            spacing: 0
            Layout.alignment: Qt.AlignVCenter
            Layout.fillHeight: true

            LogicLogoBlock {
                Layout.fillHeight: true
                onHomeRequested: root.homeRequested()
            }
            LogicToolBlock {
                Layout.fillHeight: true
                visible: root.width >= 860
            }

            LogicToolsButtonBlock {
                Layout.fillHeight: true
                onCollaborationRequested: root.collaborationRequested()
            }
        }

        Item { Layout.fillWidth: true; Layout.minimumWidth: 4 }

        RowLayout {
            spacing: 12
            Layout.alignment: Qt.AlignVCenter
            Layout.minimumWidth: (root.width >= 720) ? 260 : 220

            LogicTransportBlock { Layout.alignment: Qt.AlignVCenter }
            LogicLcdDisplayBlock {
                Layout.alignment: Qt.AlignVCenter
                visible: root.width >= 720
            }
        }

        Item { Layout.fillWidth: true; Layout.minimumWidth: 4 }

        Item { Layout.preferredWidth: 1; Layout.fillHeight: true }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: "#181A22"
    }
}