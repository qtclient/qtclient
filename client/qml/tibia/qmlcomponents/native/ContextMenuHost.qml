import QtQuick

import qmlcomponents

// Tibia's ContextMenuTemplate at the click position, over a full-window area
// that closes the menu when the user clicks anywhere else.
Item {
  id: host

  property QtObject controller: null

  anchors.fill: parent

  MouseArea {
    anchors.fill: parent
    acceptedButtons: Qt.AllButtons
    onPressed: controller.requestClose()
    onWheel: controller.requestClose()
  }

  ContextMenuTemplate {
    controller: host.controller
    x: Math.max(0, Math.min(host.controller.position.x, host.width - width))
    y: Math.max(0, Math.min(host.controller.position.y, host.height - height))
  }
}
