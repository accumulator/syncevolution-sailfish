import QtQuick 2.0
import Sailfish.Silica 1.0
import org.syncevolution.qml 1.0

Page {
    id: page
    allowedOrientations: Orientation.All

    property string peerId
    property string peerName
    property string reportDir

    SilicaWebView {
        id: view
        anchors.fill: parent
        url: reportDir + "/syncevolution-log.html"
    }
}
