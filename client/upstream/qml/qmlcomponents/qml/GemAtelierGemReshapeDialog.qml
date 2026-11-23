import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import qmlcomponents

TibiaDialog {
  id: root

  caption: qsTrId("skill_wheel_mod_reshape_dialog_caption")
  width: 574

  initialFocusItem: root
  KeyNavigation.tab: root
  KeyNavigation.backtab: root

  required property var controller
  property var currentMod: controller.currentMod
  property var reshapedMod: controller.reshapedMod

  readonly property int perkCardWidth: 300

  component ModComponent : Item {
    id: modComponent
    height: 130
    width: 200

    property string modEffect: ""
    property int modGrade: 0
    property url modSource: ""
    property bool isSupremeMod: false

    readonly property bool hasMod: modComponent.modSource != ""

     Image {
       id: modIcon
       anchors.top: parent.top
       anchors.topMargin: TibiaStyle.marginUnrelated
       anchors.horizontalCenter: parent.horizontalCenter
       smooth: false
       source: root.controller.getModGradeImageSource(modComponent.modGrade)

       Image {
         id: modImage
         smooth: false
         source: modComponent.modSource
         anchors.centerIn: modIcon
         anchors.horizontalCenterOffset: isSupremeMod ? 3 : 0
         anchors.verticalCenterOffset: isSupremeMod ? -2 : 0
       } // Image

       Image {
         id: gradeHighlightImage
         source: root.controller.getModGradeHighlightImageSource(modComponent.modGrade)
         visible: false
       } // Image
     } // Image

     TibiaText {
       anchors.top: modIcon.bottom
       anchors.topMargin: TibiaStyle.marginUnrelated
       anchors.horizontalCenter: modIcon.horizontalCenter
       text: modEffect

     }

  } // component

  ColumnLayout {
    id: mainLayout
    anchors.left: parent.left
    anchors.right: parent.right
    spacing: TibiaStyle.marginUnrelated

    RowLayout {
      id: gemRowLayout
      spacing: TibiaStyle.marginUnrelated

      ColumnLayout {
        spacing: TibiaStyle.marginUnrelated

        TibiaPanel2PixelUpFilledWithCaption {
          Layout.fillWidth: true
          Layout.preferredHeight: 130 + 2*marginsToContent

          caption: qsTrId("skill_wheel_mod_reshape_dialog_caption_current")

          ModComponent {
            id: oldMod
            Layout.alignment: Qt.AlignHCenter
            modGrade:  currentMod != null ? currentMod.modGrade : 0
            modSource: currentMod != null ? currentMod.imageSource : null
            modEffect: currentMod != null ? currentMod.effectString : ""
            isSupremeMod: currentMod != null ? currentMod.isSupremeMod : false
          } // ModComponent

        } //TibiaPanel2PixelUpFilledWithCaption
        
        
        TibiaButton {
          Layout.preferredWidth: root.perkCardWidth / 2
          Layout.preferredHeight: TibiaStyle.buttonHeightHigh
          Layout.alignment: Qt.AlignHCenter

          text: qsTrId("skill_wheel_mod_reshape_dialog_button_keep")
          textFont: TibiaStyle.defaultTextFont
          onClicked: controller.requestCancel()
        } //TibiaButton
      } //ColumnLayout

      TibiaVerticalSeparator {
        Layout.fillHeight: true
      } // TibiaHorizontalSeparator

      ColumnLayout {
        id: newModLayout
        spacing: TibiaStyle.marginUnrelated

        TibiaPanel2PixelUpFilledWithCaption {
          Layout.fillWidth: true
          Layout.preferredHeight: 130 + 2*marginsToContent

          caption: qsTrId("skill_wheel_mod_reshape_dialog_caption_new")

         ModComponent {
           id: newMod
           Layout.alignment: Qt.AlignHCenter
           modGrade: reshapedMod != null ? reshapedMod.modGrade : 0
           modSource: reshapedMod != null ? reshapedMod.imageSource : null
           modEffect: reshapedMod != null ? reshapedMod.effectString : ""
           isSupremeMod: reshapedMod != null ? reshapedMod.isSupremeMod : 0
          } // ModComponent
        } //TibiaPanel2PixelUpFilledWithCaption

        TibiaButton {
          Layout.preferredWidth: root.perkCardWidth / 2
          Layout.preferredHeight: TibiaStyle.buttonHeightHigh
          Layout.alignment: Qt.AlignHCenter

          text: qsTrId("skill_wheel_mod_reshape_dialog_button_new")
          textFont: TibiaStyle.defaultTextFont
          onClicked: {
            controller.chooseNewMod();
          } //onClicked

          TibiaRectangleHighlight {}
        } //TibiaButton


      } //ColumnLayout
    } //RowLayout

    Item {
      id: highlightVisibleEnabler
      Layout.fillWidth: true
      Layout.preferredHeight: 1
      Layout.topMargin: -2*parent.spacing
    } //Item
  } // ColumnLayout
} // TibiaDialog
