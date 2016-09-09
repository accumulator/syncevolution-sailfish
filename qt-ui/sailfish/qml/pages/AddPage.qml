import QtQuick 2.0
import Sailfish.Silica 1.0
import org.syncevolution.qml 1.0

Page {
    id: page
    allowedOrientations: Orientation.All

    SilicaListView {
        id: view
        anchors.fill: parent

        model: SyncEvo.templateModel

        header: PageHeader {
            title: qsTr("Service templates")
        }

        ViewPlaceholder {
            enabled: view.count == 0
            text: qsTr("No templates")
        }

        delegate: ListItem {
            id: listItem
            width: view.width
            contentHeight: Theme.itemSizeSmall

            Label {
                id: label
                x: Theme.paddingLarge
                text: peerName
                color: listItem.highlighted ? Theme.highlightColor : Theme.primaryColor
                font.pixelSize: Theme.fontSizeExtraLarge
            }

            onClicked: {
                pageStack.push("EditPage.qml",
                               {"acceptDestination": previousPage(page),
                                "acceptDestinationAction": PageStackAction.Pop,
                                "templateId": peerId,
                                "templateName": peerName})
            }
        }

        VerticalScrollDecorator {}
    }
}
