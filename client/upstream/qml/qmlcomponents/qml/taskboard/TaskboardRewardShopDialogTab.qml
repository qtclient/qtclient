import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtWebChannel
import QtWebEngine

import qmlcomponents
import "qrc:/qt/qml/qmlcomponents/qml/"
import QtQuick.LegacyControls
import qmlenumvalues


TibiaFrame2PixelUpFilled {

  id: root
  required property var controller
  property var rewardOffers: controller != null ? controller.rewardList : null
  
  TibiaFrame1PixelDown {
    anchors.fill: parent
    anchors.margins: TibiaStyle.dialogContentToFrameMargin
  
    BorderImage {
      smooth: false
      anchors.fill: parent
      anchors.margins: 1
      source: "/images/background-dark.png"
      horizontalTileMode: BorderImage.Repeat
      verticalTileMode: BorderImage.Repeat
      visible: true
      cache: false
  
      TibiaScrollView {
        id: containerScrollView
        
        anchors.fill: parent
        anchors.margins: 0
        
      
        GridView {
          id: rewardGrid
          model: rewardOffers
          topMargin: TibiaStyle.marginUnrelated
          leftMargin: TibiaStyle.marginRelated
          bottomMargin: 0
          anchors.centerIn: parent
      
          boundsBehavior: Flickable.StopAtBounds
          interactive: false //prevent flick behavior on touch screens
          footer: Item {
            height: TibiaStyle.marginNarrow
            width: TibiaStyle.marginRelated * 2
          }
          
          cellHeight: 146 + TibiaStyle.marginUnrelated
          cellWidth: 296 + TibiaStyle.marginUnrelated
      
          delegate: TibiaFrame2PixelUpFilledWithCaption {
            
            id: offerFrame
            // Qt 6 required-property role injection — more reliable than model.* for custom roles.
            required property int index
            required property int rewardType
            required property int appearanceId
            required property int optionalId
            required property string title
            required property string description
            required property int cost
            required property string costString
            required property bool canBuy
            required property bool alreadyOwned
            required property string noBuyReasonString
            required property bool addOn1
            required property bool addOn2
            property int itemIndex: index
            width: 296
            height: 146
            caption: title
            
            ColumnLayout {
              id: singleRewardInfo
              
              anchors.fill: parent
              anchors.leftMargin: TibiaStyle.marginUnrelated
              anchors.rightMargin: TibiaStyle.marginUnrelated
              anchors.bottomMargin: TibiaStyle.marginUnrelated
              anchors.topMargin: TibiaStyle.marginUnrelated + offerFrame.captionHeight
              spacing: TibiaStyle.marginUnrelated
              
              RowLayout {
            
                TaskboardShopOffer {
                  
                  rewardType: offerFrame.rewardType
                  appearanceId: offerFrame.appearanceId
                  optionalAppearanceId: offerFrame.optionalId
                  outfitAppearanceId: offerFrame.appearanceId
                  headColor: controller != null ? controller.headColor : "black"
                  torsoColor: controller != null ? controller.torsoColor : "black"
                  legsColor: controller != null ? controller.legsColor : "black"
                  detailColor: controller != null ? controller.detailColor : "black"
                  addOn1: offerFrame.addOn1
                  addOn2: offerFrame.addOn2
                }
            
                           
                ColumnLayout {
                  Layout.fillWidth: true
                  Layout.alignment: Qt.AlignTop
                  Layout.rightMargin: TibiaStyle.marginUnrelated*2
                
                  TibiaText {
                    text: offerFrame.description
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                  } // TibiaText
                
                } // ColumnLayout
              } // RowLayout
              
              TibiaHorizontalSeparator {
                Layout.fillWidth: true
              }
              
              RowLayout {
              
                Layout.preferredHeight: 20
                
                // buy button
                TibiaButton {
                  text: qsTrId("buy")
                  Layout.preferredWidth: 64
                  Layout.preferredHeight: 20
                  enabled: controller != null && (controller.huntingTaskPoints >= offerFrame.cost) && offerFrame.canBuy
                  visible: !offerFrame.alreadyOwned
                  tooltipText: offerFrame.noBuyReasonString
                
                  onClicked: {
                    if (controller != null) {
                      controller.requestBuyOffer(itemIndex);
                    }
                  } //onClicked
                  
                } //TibiaButton
                
                TibiaFrame1PixelDown {
                  Layout.preferredWidth: 64
                  Layout.preferredHeight: 20
                  visible: offerFrame.alreadyOwned
                  
                  RowLayout {
                    anchors.centerIn: parent
                    Image {
                      source: "/images/icon-yes.png"
                    } // Image
                  
                    TibiaText {
                      text: qsTrId("taskboard_shop_bought_text")
                      font: TibiaStyle.buttonFont
                      styleType: "TabCaptionIdle"//"Disabled"
                    } // TibiaText
                  } // RowLayout
                  
                } // TibiaFrame1PixelDown
                
                
                TibiaCurrencyView {
                  Layout.preferredWidth: TibiaStyle.currencyViewWidth
                  Layout.preferredHeight: 20
                  balance: offerFrame.costString
                  rightAligned: true
                  iconId: "PreyHuntingTaskTokens"
                } // TibiaCurrencyView
                
                Item {
                  Layout.fillWidth: true
                }
                
                Image {
                  source: (offerFrame.rewardType === TaskboardRewardListModel.Outfit) ? "/images/taskboard/backdrop_huntingtaskpoint_shop_outfit.png" 
                           : ( offerFrame.rewardType === TaskboardRewardListModel.Mount ? "/images/taskboard/backdrop_huntingtaskpoint_shop_Mount.png" 
                           : ( offerFrame.rewardType === TaskboardRewardListModel.PromotionPoint ? "/images/taskboard/backdrop_huntingtaskpoint_shop_boost.png" :
                           "/images/taskboard/backdrop_huntingtaskpoint_shop_decoration.png")) 
                  Layout.alignment: Qt.AlignRight
                  
                  Layout.rightMargin: -13
                } //Image
                
                //backdrop_huntingtaskpoint_shop_outfit
                
              } // RowLayout
            
            } // ColumnLayout
      
      
          } // TibiaFrame2PixelUpFilled (delegate)
          
        } // GridView
      }  //TibiaScrollView
    } // BorderImage
  } // TibiaFrame1PixelDown
} // TibiaFrame2PixelUpFilled




