import QtQuick 2.0
import Sailfish.Silica 1.0
import org.syncevolution.qml 1.0

Page {
    id: page
    allowedOrientations: Orientation.All

    property string peerId
    property string peerName

    SilicaListView {
        id: view
        anchors.fill: parent

        model: SyncEvo.getReportModel(peerId)

        header: PageHeader {
            title: qsTr("History for %1").arg(peerName)
        }

        ViewPlaceholder {
            enabled: view.count == 0
            text: qsTr("No history")
        }

        delegate: ListItem {
            id: listItem
            width: view.width
            contentHeight: column.height

            Column {
                id: column
                width: parent.width

                Label {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: Theme.paddingMedium
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.secondaryColor
                    text: reportStart
                }

                Label {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    font.pixelSize: Theme.fontSizeSmall
                    wrapMode: Text.Wrap
                    text: reportStatus + " " + reportError
                }
            }

            onClicked: {
                pageStack.push("LogPage.qml",
                               {"peerId": peerId,
                                "peerName": peerName,
                                "reportDir": reportDir})
            }
        }

        VerticalScrollDecorator {}
    }
}
