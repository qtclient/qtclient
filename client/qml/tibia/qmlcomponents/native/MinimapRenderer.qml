import QtQuick

// Automap view used by the cyclopedia map and house dialogs.
Item {
  id: renderer

  property bool showAreaCaptions: false
  property bool showExploreOverlay: false
  property bool showZones: false
  property bool showNPCMarkers: true
  property bool showPassages: true
  property bool showHouses: false
  property bool levelSeparator: false
  property int tileType: 0
  property int userSelectedTileType: 0
  property var minimapMarkersFilter: []
  property real scaleFactor: 1.0
  property var cameraPosition: Qt.point(0, 0)
  property int floor: 7
  property bool smoothTextureFiltering: false

  readonly property QtObject camera: QtObject {
    property real scaleFactor: renderer.scaleFactor
    property point referencePoint: Qt.point(0, 0)
    property point topLeftSubfieldCoordinate: Qt.point(0, 0)
    property int layer: renderer.floor

    function moveDistanceRelativeToReferencePoint(offset) {
      topLeftSubfieldCoordinate = Qt.point(topLeftSubfieldCoordinate.x - offset.x / scaleFactor,
                                           topLeftSubfieldCoordinate.y - offset.y / scaleFactor);
    }
    function changeLayerWithIsometricProjection(newLayer) {
      layer = newLayer;
    }
  }

  signal hoverCoordinateChanged(var coordinate)
  signal clickCoordinateChanged(var coordinate)

  function updateHoverCoordinate(x, y) {}
  function updateClickCoordinate(x, y) {}
}
