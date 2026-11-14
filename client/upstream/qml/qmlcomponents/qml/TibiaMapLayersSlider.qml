import QtQuick
import qmlcomponents




Item {
  id: mapLayersSlider
  property int currentLayer: 0
  property int minimumLayer: 0
  property int maximumLayer: 15
  property bool noLayers: false

  property alias tooltip: layerSliderTooltip.text

  height: layersCanvas.height
  width: 20

  Component.onCompleted: layersCanvasRequestPaintTimer.restart()
  onMinimumLayerChanged: layersCanvasRequestPaintTimer.restart()
  onMaximumLayerChanged: layersCanvasRequestPaintTimer.restart()
  onNoLayersChanged: layersCanvasRequestPaintTimer.restart()
  onCurrentLayerChanged: {
    correctLayers();
    layersCanvas.requestPaint();
    indicator.y = currentLayer * 4;
  } //onCurrentLayerChanged

  Timer {
    id: layersCanvasRequestPaintTimer
    interval: 0
    repeat: false
    onTriggered: {
      correctLayers();
      layersCanvas.requestPaint()
    }
  } //Timer

  function correctLayers() {
    var newMinimumLayer = Math.max(0, minimumLayer);
    var newMaximumLayer = Math.max(newMinimumLayer, Math.min(maximumLayer, 15));
    var newCurrentLayer = Math.min(newMaximumLayer, Math.max(newMinimumLayer, mapLayersSlider.currentLayer));

    if (newMinimumLayer != minimumLayer) {
      minimumLayer = newMinimumLayer;
    }
    if (newMaximumLayer != maximumLayer) {
      maximumLayer = newMaximumLayer;
    }
    if (newCurrentLayer != currentLayer) {
      currentLayer = newCurrentLayer;
    }
  } //function correctLayers

  function moveLayer(direction)
  {
    if (direction > 0) {
      mapLayersSlider.currentLayer = mapLayersSlider.currentLayer - 1;
    } else if (direction < 0) {
      mapLayersSlider.currentLayer = mapLayersSlider.currentLayer + 1;
    }
  } //function moveLayer

  function setLayerValue(value)
  {
    mapLayersSlider.currentLayer = value
  } //function setLayerValue

  function findNearestLayerValue(y)
  {
    y = Math.max(0, Math.min(dragArea.height, y));
    var nearestLayerValue = Math.abs(parseInt((y - 3) / 4));
    return nearestLayerValue;
  } //function findNearestLayerValue

  MouseArea {
    id: dragArea
    property bool _dragged: false
    property var _clickPoint: Qt.point(0,0)
    anchors.fill: parent
    acceptedButtons: Qt.LeftButton | Qt.RightButton
    hoverEnabled: false

    onWheel: (wheel) => {
      moveLayer(wheel.angleDelta.y);
    } //onWheel

    onPositionChanged: (mouse) => {
      if (mouse.buttons == Qt.LeftButton) {
        var dragDistance = Math.sqrt(Math.pow(_clickPoint.x - mouse.x, 2) + Math.pow(_clickPoint.y - mouse.y, 2));
        if (_dragged == true || dragDistance >= TibiaStyle.dragThreshold) {
          _dragged = true;
          setLayerValue(findNearestLayerValue(mouse.y));
        }
      }
    } //onPositionChanged

    onPressed: (mouse) => {
      if (mouse.buttons == Qt.LeftButton) {
        _dragged = false
        _clickPoint = Qt.point(mouse.x, mouse.y);
      }
    } //onPressed

    onReleased: (mouse) => {
      if (mouse.button == Qt.LeftButton) {
        if (_dragged == false) {
          var clickedLayer = findNearestLayerValue(mouse.y);
          if (clickedLayer >= 0 && clickedLayer < 7) {
            moveLayer(1);
          } else if (clickedLayer > 7 && clickedLayer <= 15) {
            moveLayer(-1);
          }
        }
      }
    } //onReleased
    z: 999

    Tooltip {
      id: layerSliderTooltip
      enabled: true
      anchors.fill: parent
    } //Tooltip
  } //MouseArea

  TibiaVerticalSeparator {
    anchors.top: parent.top
    anchors.topMargin: 3
    anchors.bottom: parent.bottom
    anchors.bottomMargin: 3
    anchors.right: parent.right
    anchors.rightMargin: 2
    visible: !noLayers
  } //TibiaVerticalSeparator

  Image {
    id: indicator
    source: "/images/automap-indicator-slider-left.png"
    anchors.right: parent.right
    smooth: false
    visible: !noLayers
  } //Image

  Canvas {
    id: layersCanvas
    anchors.left: parent.left
    height: 67
    width: 14
    antialiasing: false
    smooth: false

    property int selectedIndex: mapLayersSlider.currentLayer
    property int minimumLayer: mapLayersSlider.minimumLayer
    property int maximumLayer: mapLayersSlider.maximumLayer
    property bool isActive: !mapLayersSlider.noLayers

    // --- constants --------------------------------------------------------

    readonly property int layerCount: 16
    readonly property int layerSpacing: 4 // vertical offset per layer
    readonly property int shadowlessLayer: layerCount - 1

    // b = border, f = fill, h = highlight, s = shadow, space = transparent
    readonly property var shapeRows: [
      "      bb      ",
      "    bbffbb    ",
      "  bbffffffbb  ",
      "bbhhffffffffbb",
      "  bbhhffffbb  ",
      "    bbhhbb    ",
      "    ssbbss    ",
      "     ssss     "
    ]
    readonly property int shapeWidth: shapeRows[0].length
    readonly property int shapeHeight: shapeRows.length

    readonly property string disabledBorder: "#aa414141"
    readonly property string disabledFill:   "#aa939393"

    // one { border, fill } per layer, built once at startup
    readonly property var palettes: buildPalettes()

    // "<index>|<variant>" -> CanvasImageData
    property var imageCache: ({})

    // --- repaint triggers -------------------------------------------------

    onSelectedIndexChanged: requestPaint()
    onMinimumLayerChanged: requestPaint()
    onMaximumLayerChanged: requestPaint()
    onIsActiveChanged: requestPaint()

    // --- palette ----------------------------------------------------------

    function buildPalettes() {
      var groups = [
        { count: 7, border: "#2E4955", fill: "#64A8C3" },   // blue
        { count: 1, border: "#303B20", fill: "#708C44" },   // green
        { count: 8, border: "#3B2820", fill: "#8C5944" }    // brown
      ];

      var result = [];

      for (var g = 0; g < groups.length; ++g) {
        for (var i = 0; i < groups[g].count; ++i) {
          // darker within each group
          var factor = 1.0 + i * 0.1;

          result.push({
            border: Qt.darker(groups[g].border, factor),
            fill: Qt.darker(groups[g].fill, factor),
          });
        }
      }

      return result;
    }

    function variantOf(index) {
      if (!isActive || index < minimumLayer || index > maximumLayer) {
        return "disabled";
      }

      return index === selectedIndex ? "selected" : "normal";
    }

    function paletteOf(index, variant) {
      if (variant !== "disabled") {
        return palettes[index];
      }

      var factor = 1.0 + index * 0.1;

      return {
        border: Qt.darker(disabledBorder, factor),
        fill: Qt.darker(disabledFill, factor),
      };
    }

    // --- pixel data -------------------------------------------------------

    function toBytes(color, alphaScale) {
      var a = (alphaScale === undefined) ? 1.0 : alphaScale;
      return [
        Math.round(color.r * 255),
        Math.round(color.g * 255),
        Math.round(color.b * 255),
        Math.round(color.a * a * 255)
      ];
    }

    function buildLayerImage(ctx, index, variant) {
      var palette  = paletteOf(index, variant);
      var selected = (variant === "selected");

      var channels = {
        "b": toBytes(Qt.lighter(palette.border, selected ? 2.5 : 1.0)),
        "f": toBytes(Qt.lighter(palette.fill, selected ? 2.0 : 1.0)),
        "h": toBytes(Qt.lighter(palette.fill, selected ? 3.0 : 1.5))
      };

      if (index !== shadowlessLayer) {
        channels["s"] = toBytes(palette.border, 0.5);
      }

      var transparent = [0, 0, 0, 0];
      var image = ctx.createImageData(shapeWidth, shapeHeight);
      var data = image.data;

      for (var y = 0; y < shapeHeight; ++y) {
        var row = shapeRows[y];
        for (var x = 0; x < shapeWidth; ++x) {
          var bytes = channels[row.charAt(x)] || transparent;
          var p = (y * shapeWidth + x) * 4;
          data[p]     = bytes[0];
          data[p + 1] = bytes[1];
          data[p + 2] = bytes[2];
          data[p + 3] = bytes[3];
        }
      }

      return image;
    }

    function layerImage(ctx, index) {
      var variant = variantOf(index);
      var key = index + "|" + variant;
      if (!imageCache[key]) {
        imageCache[key] = buildLayerImage(ctx, index, variant);
      }
      return imageCache[key];
    }

    // --- paint ------------------------------------------------------------

    onPaint: {
      correctLayers();

      var ctx = getContext('2d');
      ctx.clearRect(0, 0, width, height);

      // highest index first, so lower layers end up drawn on top
      for (var i = layerCount - 1; i >= 0; --i) {
        ctx.drawImage(layerImage(ctx, i), 0, i * layerSpacing);
      }
    } //onPaint
  } //Canvas
} //Item
