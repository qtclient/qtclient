import QtQuick
import QtQuick.Controls
import QtWebEngine

TibiaDialog {
  id: root
  required property QtObject controller
  property url targetUrl: controller.targetUrl

  caption: qsTrId("cloudflare_challenge_title")
  width: 800

  WebEngineView {
    id: view
    anchors.left: parent.left
    anchors.right: parent.right
    height: 500
    profile: root.controller.webProfile

    function onStopSolverWebview() {
      // instantly halt any further network requests or redirects
      view.stop()
    }
  }

  Component.onCompleted: {
    // load as soon as it opens
    view.url = root.targetUrl
  }
}
