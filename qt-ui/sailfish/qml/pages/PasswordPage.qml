import QtQuick 2.0
import Sailfish.Silica 1.0
import org.syncevolution.qml 1.0
import "common"

Dialog {
    id: page
    allowedOrientations: Orientation.All

    property SyncEvoInfoRequest request
    property SyncEvoPeer peer
    property alias peerName: peerNameText.text
    property alias username: usernameText.text
    property alias password: passwordText.text

    onOpened: {
        // seems "description" is currently the peer ID.
        peer = SyncEvo.getPeer(request.getParameter("description"))
        peerName = peer.name
        username = request.getParameter("user")
    }

    onAccepted: {
        request.setResponse("password", password)
        request.sendResponse()
    }

    onRejected: {
        request.sendResponse()
    }

    Column {
        width: parent.width

        PageHeader {
            id: header
            title: qsTr("Enter password")
        }

        LabeledTextField {
            id: peerNameText
            width: parent.width
            label: qsTr("Service Name")
            readOnly: true
        }

        LabeledTextField {
            id: usernameText
            width: parent.width
            label: qsTr("Username")
            readOnly: true
        }

        LabeledTextField {
            id: passwordText
            width: parent.width
            label: qsTr("Password")
            echoMode: TextInput.Password
            inputMethodHints: Qt.ImhNoAutoUppercase
            enterIconSource: "image://theme/icon-m-enter-accept"
            onEnterClicked: accept()
        }
    }
}
