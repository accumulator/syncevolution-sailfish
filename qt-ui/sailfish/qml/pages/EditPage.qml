import QtQuick 2.0
import Sailfish.Silica 1.0
import org.syncevolution.qml 1.0
import "common"
import "common/Tables.js" as Tables

Dialog {
    id: page
    allowedOrientations: Orientation.All

    property string templateId
    property string templateName
    property bool isCreating: templateId != ""
    property SyncEvoSession storeSession
    property SyncEvoPeer peer
    property string peerId
    property alias peerName: peerNameText.text
    property alias syncUrl: syncUrlText.text
    property alias username: usernameText.text
    property alias password: passwordText.text
    property alias autoSyncEnable: autoSyncSwitch.checked
    property alias autoSyncInterval: autoSyncIntervalText.text
    property alias autoSyncDelay: autoSyncDelayText.text

    // Currently, to keep things simple, we use peerName to generate peerId when creating a new service.
    canAccept: isCreating ? (peerName != "" && !SyncEvo.existsPeer(peerName + "@" + peerName)) : true

    onOpened: {
        if (isCreating) {
            peer = SyncEvo.getTemplate(templateId)
            peerName = templateName
        }
        else {
            peer = SyncEvo.getPeer(peerId)
        }
        storeSession = peer.getTempSyncSession()
        sourceRepeater.model = peer.sourceModel
        storeRepeater.model = peer.storeModel
        syncUrl = peer.getServerConfig("syncURL")
        username = peer.getServerConfig("username")
        password = peer.getServerConfig("password")
        autoSyncEnable = peer.getSyncConfig("autoSync") == "1"
        autoSyncInterval = peer.getSyncConfig("autoSyncInterval")
        autoSyncDelay = peer.getSyncConfig("autoSyncDelay")
        for (var i=0; i<sourceRepeater.count; i++) {
            var item = sourceRepeater.itemAt(i)
            var sourceId = item.source.sourceId
            item.sourceUri = peer.getSourceUri(sourceId)
        }
        for (var i=0; i<storeRepeater.count; i++) {
            var item = storeRepeater.itemAt(i)
            var storeId = item.store.storeId
            var modeText = peer.getStoreConfig(storeId, "sync")
            item.storeModeIndex = Tables.indexOfSyncMode(modeText)
            item.databaseModel = storeSession.getDatabaseModel(storeId)
            // The onCountChanged will handle selecting the right database
            // after the model is loaded (it might be loaded asynchronously)
        }
    }

    onDone: {
        SyncEvo.closeSession(storeSession)
    }

    onAccepted: {
        if (isCreating) {
            peer = SyncEvo.createPeer(peerName + "@" + peerName, peerName, templateId)
        }
        peer.setServerConfig("syncURL", syncUrl)
        peer.setServerConfig("username", username)
        peer.setServerConfig("password", password)
        peer.setSyncConfig("autoSync", autoSyncEnable ? "1" : "")
        peer.setSyncConfig("autoSyncInterval", autoSyncInterval)
        peer.setSyncConfig("autoSyncDelay", autoSyncDelay)
        for (var i=0; i<sourceRepeater.count; i++) {
            var item = sourceRepeater.itemAt(i)
            var sourceId = item.source.sourceId
            peer.setSourceUri(sourceId, item.sourceUri)
        }
        for (var i=0; i<storeRepeater.count; i++) {
            var item = storeRepeater.itemAt(i)
            var storeId = item.store.storeId
            var modeText = Tables.syncModeOfIndex(item.storeModeIndex)
            peer.setStoreConfig(storeId, "sync", modeText)
            if (item.databaseCount) {
                var databaseId = item.databaseModel.idOfIndex(item.databaseIndex)
                peer.setStoreConfig(storeId, "database", databaseId)
            }
        }
        peer.commitConfig()
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height

        Column {
            id: column
            width: parent.width

            PageHeader {
                id: header
                title: isCreating ? qsTr("Create service") : qsTr("Edit service")
            }

            LabeledTextField {
                visible: isCreating
                id: templateNameText
                width: parent.width
                label: qsTr("Template")
                text: templateName
                readOnly: true
            }

            LabeledTextField {
                id: peerNameText
                width: parent.width
                label: qsTr("Service Name")
                readOnly: !isCreating
            }

            Label {
                anchors.left: parent.left
                anchors.leftMargin: Theme.paddingLarge
                anchors.right: parent.right
                anchors.rightMargin: Theme.paddingLarge
                font.pixelSize: Theme.fontSizeTiny
                text: qsTr("\
Note that your password is stored in cleartext in the config file. \
You can put a single dash (\"-\") in the password field to enter the password manually on every sync. \
(Suggestions about keyring daemons that could store the password are welcome.) \
OAuth2 authentication is coming soon.\n")
                color: Theme.secondaryColor
                wrapMode: Text.Wrap
            }

            LabeledTextField {
                id: usernameText
                width: parent.width
                label: qsTr("Username")
                inputMethodHints: Qt.ImhNoAutoUppercase
            }

            LabeledTextField {
                id: passwordText
                width: parent.width
                label: qsTr("Password")
                echoMode: TextInput.Password
                inputMethodHints: Qt.ImhNoAutoUppercase
            }

            LabeledTextField {
                id: syncUrlText
                width: parent.width
                label: qsTr("Sync URL")
                inputMethodHints: Qt.ImhUrlCharactersOnly
            }

            Repeater {
                id: sourceRepeater
                delegate: Column {
                    width: parent.width
                    property variant source: model
                    property alias sourceUri: sourceUriText.text

                    LabeledTextField {
                        id: sourceUriText
                        width: parent.width
                        label: qsTr("%1 DB").arg(sourceName)
                        inputMethodHints: Qt.ImhNoAutoUppercase
                    }

                    // TODO: support choosing remote WebDAV/ActiveSync databases
                }
            }

            Label {
                anchors.left: parent.left
                anchors.leftMargin: Theme.paddingLarge
                anchors.right: parent.right
                anchors.rightMargin: Theme.paddingLarge
                font.pixelSize: Theme.fontSizeTiny
                text: qsTr("\
Syncing tasks and notes is currently disabled because SyncEvolution would sync them \
to the standard calendar database, but there are not yet any Jolla apps that \
access tasks and notes stored there.\n")
                color: Theme.secondaryColor
                wrapMode: Text.Wrap
            }

            Repeater {
                id: storeRepeater
                delegate: Column {
                    width: parent.width
                    property variant store: model
                    property alias storeModeIndex: storeModeBox.currentIndex
                    property alias databaseModel: storeDatabaseRepeater.model
                    property alias databaseCount: storeDatabaseRepeater.count
                    property alias databaseIndex: storeDatabaseBox.currentIndex

                    onDatabaseCountChanged: {
                        if (databaseModel.isValid()) {
                            var databaseId = peer.getStoreConfig(storeId, "database")
                            // setting currentItem is more reliable than setting currentIndex.
                            storeDatabaseBox.currentItem = storeDatabaseRepeater.itemAt(databaseModel.indexOf(databaseId))
                        } else if (isCreating) {
                            // initially disable sync sources that don't have databases.
                            storeModeIndex = 0
                        }
                    }

                    ComboBox {
                        id: storeModeBox
                        width: parent.width
                        label: qsTr("%1 Mode").arg(storeName)
                        menu: ContextMenu {
                            MenuItem { text: qsTr("Disabled") }
                            MenuItem { text: qsTr("Two-Way") }
                            MenuItem { text: qsTr("Slow (Merge)") }
                            MenuItem { text: qsTr("Refresh From Client") }
                            MenuItem { text: qsTr("Refresh From Server") }
                            MenuItem { text: qsTr("One-Way From Client") }
                            MenuItem { text: qsTr("One-Way From Server") }
                        }
                    }

                    ComboBox {
                        id: storeDatabaseBox
                        width: parent.width
                        label: qsTr("%1 Store").arg(storeName)
                        menu: ContextMenu {
                            Repeater {
                                id: storeDatabaseRepeater
                                delegate: MenuItem {
                                    text: databaseName
                                    property variant database: model
                                }
                            }
                        }
                    }
                }
            }

            TextSwitch {
                id: autoSyncSwitch
                text: qsTr("Enable AutoSync")
                description: qsTr("Automatic synchronization at regular intervals")
            }

            Label {
                anchors.left: parent.left
                anchors.leftMargin: Theme.paddingLarge
                anchors.right: parent.right
                anchors.rightMargin: Theme.paddingLarge
                font.pixelSize: Theme.fontSizeTiny
                text: qsTr("\
Interval is the minimum time between syncs, and \
delay is the minimum time your device must have been online before starting a sync. \
To enter times, you can use suffixes, e.g. 15m, or 1h30m5s. \
(Ideas for a better UI for these fields are welcome.)\n")
                color: Theme.secondaryColor
                wrapMode: Text.Wrap
            }

            LabeledTextField {
                id: autoSyncIntervalText
                width: parent.width
                enabled: autoSyncEnable
                label: qsTr("Interval")
            }

            LabeledTextField {
                id: autoSyncDelayText
                width: parent.width
                enabled: autoSyncEnable
                label: qsTr("Delay")
            }
        }
    }
}
