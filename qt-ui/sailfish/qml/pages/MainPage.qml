import QtQuick 2.0
import Sailfish.Silica 1.0
import org.syncevolution.qml 1.0

Page {
    id: page
    allowedOrientations: Orientation.All

    Connections {
        target: SyncEvo
        onPasswordRequest: passwordPrompt(request)
    }

    function passwordPrompt(request) {
        pageStack.push("PasswordPage.qml",
                       {"request": request})
    }

    SilicaListView {
        id: view
        anchors.fill: parent

        PullDownMenu {
            MenuItem {
                text: qsTr("Add service")
                onClicked: pageStack.push("AddPage.qml")
            }
        }

        model: SyncEvo.peerModel

        header: PageHeader {
            title: qsTr("Sync services")
        }

        ViewPlaceholder {
            enabled: view.count == 0
            text: qsTr("No services")
        }

        delegate: ListItem {
            id: listItem
            menu: contextMenu
            width: view.width
            contentHeight: Theme.itemSizeSmall

            ListView.onAdd: AddAnimation { target: listItem }
            ListView.onRemove: RemoveAnimation { target: listItem }

            function remove() {
                remorseAction(qsTr("Deleting"), function() { SyncEvo.deletePeer(peerId) })
            }

            Label {
                id: label
                x: Theme.paddingLarge
                text: peerName
                color: listItem.highlighted ? Theme.highlightColor : Theme.primaryColor
                font.pixelSize: Theme.fontSizeExtraLarge
            }

            Component {
                id: contextMenu
                ContextMenu {
                    MenuItem {
                        text: qsTr("Edit service")
                        onClicked: {
                            pageStack.push("EditPage.qml",
                                           {"peerId": peerId,
                                            "peerName": peerName})
                        }
                    }
                    MenuItem {
                        text: qsTr("Delete service")
                        onClicked: remove()
                    }
                }
            }

            onClicked: {
                pageStack.push("SyncPage.qml",
                               {"peerId": peerId,
                                "peerName": peerName})
            }
        }

        VerticalScrollDecorator {}
    }
}
