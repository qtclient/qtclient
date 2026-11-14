import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtWebChannel
import QtWebEngine

import qmlcomponents
import "qrc:/qt/qml/qmlcomponents/qml/"
import QtQuick.LegacyControls
import qmlenumvalues

Item {
  id: root
  
  property int rewardType: TaskboardRewardListModel.Object
  property int appearanceId: 0
  property int optionalAppearanceId: 0
  property int outfitAppearanceId: 0
  property var headColor: "black"
  property var torsoColor: "black"
  property var legsColor: "black"
  property var detailColor: "black"
  property bool addOn1: false
  property bool addOn2: false

  implicitHeight: TibiaStyle.mapWindowPixelPerField * 2
  implicitWidth: TibiaStyle.mapWindowPixelPerField * 2
  
  TibiaFrame1PixelDown {
      
    id: frame
    height: TibiaStyle.mapWindowPixelPerField * 2
    width: TibiaStyle.mapWindowPixelPerField * 2
    anchors.centerIn: parent
    
    z: -1
    BorderImage {
      smooth: false
      anchors.fill: parent
      anchors.margins: 1
      source: "/images/backdrop-dark-grey.png"
      horizontalTileMode: BorderImage.Repeat
      verticalTileMode: BorderImage.Repeat
      visible: true
      cache: false
  
    
      Loader {
        // Explicit size: OutfitAppearanceInstanceRenderer uses anchors.fill, and a
        // zero-sized Loader collapses the preview (most shop offers are outfit/mount).
        width: TibiaStyle.mapWindowPixelPerField * 2 - 2
        height: TibiaStyle.mapWindowPixelPerField * 2 - 2
        anchors.centerIn: parent
        // Compare as int — model roles arrive as int; enum identity can fail across
        // tip vs CIP Soft QML type registrations.
        property int rewardTypeInt: Number(root.rewardType)
        
        Component {
          id: singleObjectComponent
          
          SingleObjectAppearanceInstanceRenderer {
            id: appearanceInstanceViewer
            anchors.centerIn: parent
            width: TibiaStyle.mapWindowPixelPerField
            height: TibiaStyle.mapWindowPixelPerField
          
            animated: true
            typeid: root.appearanceId
            cumulativeCount: 0
            liquidType: ObjectAppearanceInstance.EMPTY
            hookDirection: ObjectAppearanceInstance.NONE
            
          } //SingleObjectAppearanceInstanceRenderer
          
        } // Component
        
        Component {
          id: outfitComponent
          
          OutfitAppearanceInstanceRenderer {
            id: appearanceInstanceRenderer
            width: TibiaStyle.mapWindowPixelPerField * 2 - 2
            height: TibiaStyle.mapWindowPixelPerField * 2 - 2
            anchors.centerIn: parent
            shrinkToFit: true
            showNoOutfitImage: false
          
            animated: true
            outfitId: root.outfitAppearanceId
            headColor: root.headColor
            torsoColor: root.torsoColor
            legsColor: root.legsColor
            detailColor: root.detailColor
            firstAddOn: root.addOn1
            secondAddOn: root.addOn2

            Component.onCompleted: reloadOutfitAppearance()
          } //OutfitAppearanceInstanceRenderer
          
        } // Component
  
        
        Component {
          id: bedComponent
          
          
          AppearanceInstanceRenderer {
              id: bedItemRenderer
              width: TibiaStyle.mapWindowPixelPerField * 2 - 2
              height: TibiaStyle.mapWindowPixelPerField * 2 - 2
              anchors.centerIn: parent
  
              smoothTextureFiltering: false
              center: true
              animated: true
              shrinkToFit: true
  
              Component {
                id: objectComponent
  
                ObjectAppearanceInstance {
                  position: Qt.point(0, 0)
                }
              } // Component
  
              Component.onCompleted: {
                var headBoardApperanceInstance = objectComponent.createObject(bedItemRenderer);
                headBoardApperanceInstance.typeid = root.appearanceId;
                headBoardApperanceInstance.hookDirection = ObjectAppearanceInstance.SOUTH;
                
                var footBoardApperanceInstance = objectComponent.createObject(bedItemRenderer);
                footBoardApperanceInstance.typeid = root.optionalAppearanceId;
                footBoardApperanceInstance.hookDirection = ObjectAppearanceInstance.SOUTH;
                footBoardApperanceInstance.position = Qt.point(0, 32);
                
                bedItemRenderer.appearanceInstances = [headBoardApperanceInstance, footBoardApperanceInstance];
              }
            } // AppearanceInstanceRenderer
          
          
        } // Component

        Component {
          id: promotionPointComponent
          Image {

            width: TibiaStyle.mapWindowPixelPerField
            height: TibiaStyle.mapWindowPixelPerField

            source: "/images/taskboard/icon_tasksystem_promotionpoint.png"

          } // Image
        } // Component
      
        
        sourceComponent: (rewardTypeInt === TaskboardRewardListModel.Object) ? singleObjectComponent : 
          (rewardTypeInt === TaskboardRewardListModel.Outfit || rewardTypeInt === TaskboardRewardListModel.Mount) ? outfitComponent : 
           (rewardTypeInt === TaskboardRewardListModel.Bed) ? bedComponent : 
           (rewardTypeInt === TaskboardRewardListModel.PromotionPoint) ? promotionPointComponent : 
           undefined
        
      } // Loader
  
    } //Image
  } //TibiaFrame1PixelDown
  
} // Item
