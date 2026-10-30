import QtQuick

// Image fetched at runtime by key (store artwork) through a manager object.
Image {
  readonly property int tinySize13: 13
  readonly property int smallSize32: 32
  readonly property int bigSize64: 64

  property var dynamicallyLoadedImageManager: null
  property string imageKey: ""
  property size sourceImageSize: Qt.size(0, 0)
  property bool scaleUp: false

  fillMode: scaleUp ? Image.PreserveAspectFit : Image.Pad
  asynchronous: true
  smooth: false
  sourceSize: sourceImageSize.width > 0 ? sourceImageSize : undefined
  source: dynamicallyLoadedImageManager != null && imageKey.length > 0
    ? dynamicallyLoadedImageManager.imageSource(imageKey) : ""
}
