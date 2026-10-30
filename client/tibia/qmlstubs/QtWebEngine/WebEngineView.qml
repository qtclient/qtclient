import QtQuick

// No embedded browser: loads report failure so callers fall back.
Item {
    id: view

    enum LoadStatus { LoadStartedStatus, LoadStoppedStatus, LoadSucceededStatus, LoadFailedStatus }

    property url url
    property var webChannel: null
    property WebEngineProfile profile: WebEngineProfile {}
    property color backgroundColor: "transparent"

    signal loadingChanged(var loadRequest)

    function runJavaScript(script, callback) {
        if (callback)
            callback(undefined)
    }

    onUrlChanged: Qt.callLater(function() {
        view.loadingChanged({ url: view.url, status: WebEngineView.LoadFailedStatus, errorString: "web views are not supported" })
    })
}
