import QtQuick 2.0
import Sailfish.Silica 1.0

Item {
    id: item
    height: childrenRect.height

    property alias label: textLabel.text
    property alias text: textField.text
    property alias readOnly: textField.readOnly
    property alias echoMode: textField.echoMode
    property alias inputMethodHints: textField.inputMethodHints
    property string enterIconSource
    signal enterClicked

    Label {
        id: textLabel
        anchors.left: parent.left
        anchors.leftMargin: Theme.paddingLarge
        anchors.top: textField.top
        anchors.topMargin: Theme.paddingSmall
        color: item.enabled ? Theme.primaryColor : Theme.secondaryColor
    }

    TextField {
        id: textField
        enabled: item.enabled
        anchors.left: textLabel.right
        anchors.right: parent.right
        anchors.top: parent.top
        EnterKey.iconSource: enterIconSource
        EnterKey.onClicked: enterClicked()
    }
}
