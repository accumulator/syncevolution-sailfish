import QtQuick 2.0
import Sailfish.Silica 1.0
import org.syncevolution.qml 1.0
import "common"
import "common/Tables.js" as Tables

Page {
    id: page
    allowedOrientations: Orientation.All

    property SyncEvoSession session
    property string peerId
    property string peerName

    function openSession() {
        session = SyncEvo.openSession(peerId)
        log.setText(session.backlog)
    }

    function closeSession() {
        SyncEvo.closeSession(session)
    }

    Component.onCompleted: openSession()
    Component.onDestruction: closeSession()

    Connections {
        target: session
        onLogOutput: log.addText(output)
    }

    function newSession() {
        closeSession()
        openSession()
        syncMode.currentIndex = 0
    }

    function startSync() {
        var idx = syncMode.currentIndex
        if (idx == 0) {
            session.startSync()
        }
        else {
            session.startSync(Tables.syncModeOfIndex(idx))
        }
    }

    function abortSync() {
        session.abortSync()
    }

    Drawer {
        id: drawer
        anchors.fill: parent
        open: session.started
        backgroundSize: parent.height - header.height - bigButton.height

        background: [
            BusyIndicator {
                anchors.centerIn: parent
                running: session.started && !session.complete
            },
            SilicaFlickable {
                id: log
                anchors.fill: parent
                contentHeight: logText.height

                property alias text: logText.text

                TextEdit {
                    id: logText
                    width: parent.width
                    wrapMode: TextEdit.Wrap
                    font.pixelSize: Theme.fontSizeTiny
                    font.family: "Monospace"
                    color: Theme.secondaryColor
                    readOnly: true
                }

                VerticalScrollDecorator { flickable: log }

                function clearText() {
                    logText.text = ""
                }

                function addText(msg) {
                    logText.insert(logText.text.length, msg)
                    // SilicaFlickable's scrollToBottom gets buggy when
                    // calling it many times within a very short time,
                    // so try to do the scrolling ourselves
                    if (contentHeight > height) {
                        logAnimation.to = contentHeight - height
                        logAnimation.restart()
                    }
                }

                function setText(msg) {
                    logText.text = msg
                    if (contentHeight > height) {
                        logAnimation.to = contentHeight - height
                        logAnimation.restart()
                    }
                }

                SmoothedAnimation {
                    id: logAnimation
                    target: log
                    property: "contentY"

                    // values used by Silica's FastScrollAnimation
                    velocity: 4000
                    maximumEasingTime: 100
                    easing.type: Easing.InOutExpo
                }
            }
        ]

        SilicaFlickable {
            anchors.fill: parent
            contentHeight: session.started ? parent.height - drawer.backgroundSize : column.height

            Column {
                id: column
                width: parent.width

                PageHeader {
                    id: header
                    title: qsTr("Sync with %1").arg(peerName)
                }

                Button {
                    id: bigButton
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: session.complete ? qsTr("Close") : session.started ? qsTr("Abort") : qsTr("Synchronize")
                    onClicked: {
                        if (session.complete) {
                            newSession()
                        }
                        else if (session.started) {
                            abortSync()
                        }
                        else {
                            startSync()
                        }
                    }
                }

/*
                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Open Service Provider Website"
                    enabled: session.url != ""
                }
*/

                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("View Synchronization History")
                    onClicked: pageStack.push("HistoryPage.qml",
                                              {"peerId": peerId,
                                               "peerName": peerName})
                }

                ComboBox {
                    id: syncMode
                    anchors.horizontalCenter: parent.horizontalCenter
                    label: qsTr("Mode Override")
                    menu: ContextMenu {
                        MenuItem { text: qsTr("Normal") }
                        MenuItem { text: qsTr("Two-Way") }
                        MenuItem { text: qsTr("Slow (Merge)") }
                        MenuItem { text: qsTr("Refresh From Client") }
                        MenuItem { text: qsTr("Refresh From Server") }
                        MenuItem { text: qsTr("One-Way From Client") }
                        MenuItem { text: qsTr("One-Way From Server") }
                    }
                }
            }
        }
    }
}
