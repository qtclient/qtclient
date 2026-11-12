import QtQuick
import QtQml.Models
import QtQuick.Controls
import QtQuick.Layouts

import qmlcomponents

ColumnLayout {
  id: root
  property var controller
  property bool isPremium: false
  readonly property bool canBeChanged: controller.canBeChanged

  readonly property int leftColumnWidth: 146
  readonly property int middleColumnWidth: leftColumnWidth
  readonly property int maxRevealedGems: controller.maxRevealedGems

  property var selectedGem: gemSelectionModel.model.rowData(gemSelectionModel.currentIndex)
  readonly property bool hasSelectedGem: gemSelectionModel.currentIndex.valid

  spacing: TibiaStyle.marginRelated

  function releaseFocusFromSearchField() {
    root.focus = true
    searchField.focus = false
  } //function releaseFocusFromSearchField

  // The reverse lookup: which vessel (if any) currently holds a given gem, so
  // selecting a gem in the inventory grid can highlight its vessel above too.
  function vesselIndexForGemId(gemId) {
    if (gemId < 0) {
      return -1
    }
    for (let i = 0; i < root.controller.totalVesselCount; i++) {
      let socket = root.controller.socketAt(i)
      if (socket && socket.gemId === gemId) {
        return i
      }
    }
    return -1
  } //function vesselIndexForGemId

  Connections {
    target: gemSelectionModel.model

    function resetSelection() {
      gemSelectionModel.setCurrentIndex(gemSelectionModel.model.index(-1, 0), ItemSelectionModel.Clear)
    }
    function rebindSelectedGem() {
      selectedGem = Qt.binding(function() { return gemSelectionModel.model.rowData(gemSelectionModel.currentIndex); })
    }

    function onDataChanged(topLeftModelIndex, bottomRightModelIndex, roles) {
      if (!gemSelectionModel.currentIndex.valid) {
        resetSelection();
      }
      rebindSelectedGem()
      vesselRowContainer.rebindSelectedVesselGemData()
    }
    function onRowsInserted(parent, firstIndex, lastIndex) {
      resetSelection()
      rebindSelectedGem()
    }
    function onRowsRemoved(parent, firstIndex, lastIndex) {
      if (!gemSelectionModel.currentIndex.valid) {
        resetSelection()
        rebindSelectedGem()
      }
    }
    function onPaginationDataChanged() {
      if (!gemSelectionModel.currentIndex.valid) {
        resetSelection()
        rebindSelectedGem()
      }
    }
  }

  ItemSelectionModel {
    id: gemSelectionModel
    // This is the pagination model, and model.sourceModel
    // refers to the original filter model.
    model: root.controller.gemInventory

    property var shouldBeCurrentIndex: root.controller.selectedGemIndex

    onShouldBeCurrentIndexChanged: {
      if (model) {
        setCurrentIndex(shouldBeCurrentIndex, ItemSelectionModel.ClearAndSelect)
      }
    }

    onModelChanged: (model) => {
      if (model && !root.controller.selectedGemIndex.valid) {
        setCurrentIndex(model.index(-1, 0), ItemSelectionModel.ClearAndSelect);
      } else if (model && root.controller.selectedGemIndex.valid) {
        setCurrentIndex(shouldBeCurrentIndex, ItemSelectionModel.ClearAndSelect);
      }
    }

    onCurrentIndexChanged: {
      root.controller.selectedGemIndex = currentIndex;
    }
  }

  component GemAtelierSocket: TibiaFrame2PixelUpFilled {
    id: socketComponent

    property var gemData
    property var modSlotCount
    property bool selected
    property bool locked: false
    property var quality
    property bool gemSocketed
    property url bezelSource

    signal socketClicked()

    enabled: !locked
    Layout.fillWidth: true
    Layout.fillHeight: true
    Layout.minimumWidth: socketContent.implicitWidth
    Layout.preferredWidth: socketContent.implicitWidth
    Layout.preferredHeight: socketContent.implicitHeight + socketContent.anchors.topMargin + socketContent.anchors.bottomMargin

    MouseArea {
      anchors.fill: parent
      onClicked: socketComponent.socketClicked()
    } // MouseArea

    ColumnLayout {
      id: socketContent
      anchors.fill: parent
      spacing: TibiaStyle.marginUnrelated
      anchors.topMargin: TibiaStyle.marginUnrelated
      anchors.bottomMargin: TibiaStyle.marginRelated

      Item {
        Layout.alignment: Qt.AlignHCenter
        implicitWidth: bezel.implicitWidth
        implicitHeight: bezel.implicitHeight

        Image {
          anchors.centerIn: parent
          source: "/images/skillwheel/backdrop_skillwheel_socket_active.png"
          smooth: false
          visible: socketComponent.bezelSource !== ""
        }

        Image {
          id: bezel
          anchors.centerIn: parent
          source: socketComponent.bezelSource
          smooth: false

          SingleObjectAppearanceInstanceRenderer {
            anchors.centerIn: parent
            typeid: root.controller.unrevealedGemAppearanceType(
              socketComponent.quality
            )
            width: TibiaStyle.mapWindowPixelPerField
            height: width
            visible: gemSocketed
          }
        }
      }

      RowLayout {
        Layout.alignment: Qt.AlignHCenter
        spacing: 0

        Repeater {
          model: socketComponent.modSlotCount

          Item {
            id: modSlot
            required property int index
            enabled: gemSocketed

            Layout.preferredWidth: 49
            Layout.preferredHeight: 50

            Image {
              id: modIcon
              anchors.centerIn: parent
              smooth: false
              source: {
                var modGrade
                if (modSlot.index == 0)
                  modGrade = gemData.mod1Grade
                else if (modSlot.index == 1)
                  modGrade = gemData.mod2Grade
                else if (modSlot.index == 2)
                  modGrade = gemData.mod3Grade
                return root.controller.getModGradeImageSource(modGrade, gemSocketed)
              }
            } // Image

            Image {
              smooth: false
              visible: gemSocketed
              source: {
                if (!visible)
                  return ""
                if (modSlot.index == 0)
                  return gemData.mod1ImageSource || ""
                else if (modSlot.index == 1)
                  return gemData.mod2ImageSource || ""
                else if (modSlot.index == 2)
                  return gemData.mod3ImageSource || ""
              }
              anchors.centerIn: modIcon
              anchors.horizontalCenterOffset: modSlot.index == 2 ? 3 : 0
              anchors.verticalCenterOffset: modSlot.index == 2 ? -2 : 0

              Tooltip {
                anchors.fill: parent
                text: {
                  if (!gemSocketed || !gemData)
                    return ""
                  if (modSlot.index == 0)
                    return gemData.mod1EffectString || ""
                  else if (modSlot.index == 1)
                    return gemData.mod2EffectString || ""
                  else if (modSlot.index == 2)
                    return gemData.mod3EffectString || ""

                  return ""
                }
              } //Tooltip
            } // Image
          } // Item
        } // Repeater
      } // RowLayout
    } // ColumnLayout

    Rectangle {
      anchors.fill: parent
      anchors.margins: -3
      color: "transparent"
      border { color: "white"; width: 2 }
      visible: socketComponent.selected
    } // selection border

    TibiaDisabledOverlay {
      anchors.fill: parent
      visible: locked
      anchors.margins: 1
    } //TibiaDisabledOverlay
  } // component GemAtelierSocket

  TibiaText {
    id: vesselSelectionHeader
    text: qsTrId("skill_wheel_atelier_vessel_selection")
    Layout.fillWidth: true
    Layout.minimumHeight: TibiaStyle.defaultTextLineHeight + 2 * TibiaStyle.marginNarrow
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
  }

  Item {
    id: vesselRowContainer
    Layout.fillWidth: true
    Layout.fillHeight: false
    Layout.preferredHeight: vesselRow.implicitHeight + 2 * TibiaStyle.marginRelated

    property int marginsToContent: TibiaStyle.marginRelated

    property var selectedVessel: 0 <= root.controller.selectedVesselIndex && root.controller.selectedVesselIndex < root.controller.activeVesselCount
      ? root.controller.socketAt(root.controller.selectedVesselIndex) : null
    readonly property bool selectedVesselHasGem: selectedVessel != null && selectedVessel.gemId >= 0

    function gemDataForVessel(vesselIndex) {
      let vessel = root.controller.socketAt(vesselIndex)
      if (!vessel || vessel.gemId < 0) {
        return null
      }
      return root.controller.gemDataForId(vessel.gemId)
    }

    Component.onCompleted: {
      if (selectedVessel != null && root.controller.selectedVesselIndex >= root.controller.activeVesselCount) {
        root.controller.selectVessel(-1)
      }
    }

    property var selectedVesselGemData: selectedVesselHasGem
      ? gemDataForVessel(root.controller.selectedVesselIndex) : null

    function rebindSelectedVesselGemData() {
      selectedVesselGemData = Qt.binding(function() {
        return vesselRowContainer.selectedVesselHasGem
          ? vesselRowContainer.gemDataForVessel(root.controller.selectedVesselIndex) : null
      })
    }

    RowLayout {
      id: vesselRow
      anchors.fill: parent
      anchors.margins: vesselRowContainer.marginsToContent
      spacing: TibiaStyle.marginNarrow

      Repeater {
        model: root.controller.totalVesselCount

        GemAtelierSocket {
          property var socket: root.controller.socketAt(index)

          Connections {
            target: root.controller
            function onVesselChanged() {
              socket = Qt.binding(function() { return root.controller.socketAt(index) })
            }
          }

          gemData: socket ? root.controller.gemDataForId(socket.gemId) : null
          modSlotCount: socket ? socket.modSlotCount : null
          quality: socket ? socket.vesselQuality : -1
          selected: index === root.controller.selectedVesselIndex
          locked: index >= root.controller.activeVesselCount
          gemSocketed: socket ? socket.gemId >= 0 : false
          bezelSource: socket ? socket.socketBezelSource : ""

          onSocketClicked: {
            if (index === root.controller.selectedVesselIndex) {
              root.controller.selectVessel(-1)
              gemSelectionModel.clearCurrentIndex()
              gemsGrid.currentIndex = -1
              gemSelectionModel.model.sourceModel.filterGemQuality = -1
            } else {
              root.controller.selectVessel(index)
              if (socket) {
                gemSelectionModel.model.sourceModel.filterGemQuality = socket.vesselQuality
                root.controller.selectGem(socket.gemId)
                releaseFocusFromSearchField()
              }
            }
          }
        } // GemAtelierSocket
      } // Repeater
    } // RowLayout
  } // Item

  RowLayout {
    Layout.fillWidth: true
    Layout.alignment: Qt.AlignTop

    // Lower middle column: gem modification actions
    TibiaFrame2PixelUpFilledWithCaption {
      caption: qsTrId("skill_wheel_atelier_gem_modification")
      Layout.preferredWidth: root.middleColumnWidth
      Layout.fillHeight: true

      TibiaGuiHelp {
        anchors { right: parent.right; top: parent.top;
                  topMargin: parent.borderWidth + 1;
                  rightMargin: parent.borderWidth + TibiaStyle.marginNarrow; }
        text: qsTrId("skill_wheel_atelier_reshape_tooltip")
      }

      ColumnLayout {
        id: gemActionsColumn

        anchors {
          fill: parent
          margins: parent.marginsToContent
          topMargin: parent.topMarginToContent
        }
        spacing: TibiaStyle.marginNarrow

        Connections {
          target: root.controller
          function onVesselChanged() {
            vesselRowContainer.selectedVessel = Qt.binding(function() {
              return 0 <= root.controller.selectedVesselIndex && root.controller.selectedVesselIndex < root.controller.activeVesselCount
                ? root.controller.socketAt(root.controller.selectedVesselIndex) : null
            })
            vesselRowContainer.rebindSelectedVesselGemData()
          }
        }

        TibiaButton {
          id: socketButton
          Layout.fillWidth: true
          property var mode: {
            if (vesselRowContainer.selectedVesselHasGem)
              if (gemSelectionModel.currentIndex.valid && root.selectedGem.gemId != vesselRowContainer.selectedVessel.gemId)
                return "replace"
              else
                return "unsocket"
            else if (gemSelectionModel.currentIndex.valid && root.selectedGem.isSocketed && (
                vesselRowContainer.selectedVessel == null || root.selectedGem.gemId == vesselRowContainer.selectedVessel.gemId)
              )
              return "unsocket"
            else
              return "socket"
          }
          text: qsTrId("skill_wheel_atelier_" + mode)
          enabled: root.isPremium && root.canBeChanged
            && (mode != "socket" || gemSelectionModel.currentIndex.valid && vesselRowContainer.selectedVessel)

          onClicked: {
            const gemId = root.selectedGem.gemId
            root.controller.socketOrUnsocketGem(gemId, root.controller.selectedVesselIndex)
          }

          Tooltip {
            anchors.fill: parent
            text: {
              if (socketButton.enabled || !root.hasSelectedGem) {
                return ""
              }
              let messages = []
              if (!root.isPremium) {
                messages.push(qsTrId("skill_wheel_atelier_socket_not_premium"))
              }
              if (!root.canBeChanged) {
                messages.push(qsTrId("skill_wheel_atelier_socket_not_in_temple"))
              }
              return messages.join("\n")
            }
          } // Tooltip
        } // TibiaButton

        component ModReshapeComponent : Item {
          id: modReshapeComponent
          Layout.preferredHeight: reshapeContent.implicitHeight
          Layout.fillWidth: true

          property string modEffect
          property int modId
          property int modGrade: 0
          property url modSource: ""
          property int costGold
          property var goldBalance: storeAndResourceBalanceHelper.totalGoldBalance
          property int costFragments
          property int fragmentBalance: isSupremeMod ? storeAndResourceBalanceHelper.greaterFragmentCount : storeAndResourceBalanceHelper.lesserFragmentCount
          property int modIdx
          readonly property bool isSupremeMod: modIdx == 2
          readonly property bool hasMod: modReshapeComponent.modSource != ""

          ColumnLayout {
            id: reshapeContent
            visible: parent.hasMod

            anchors.fill: parent

            TibiaText {
              text: modEffect
              Layout.fillWidth: true
              wrapMode: Text.Wrap
              Layout.minimumHeight: TibiaStyle.defaultTextLineHeight * 3
              Layout.maximumHeight: TibiaStyle.defaultTextLineHeight * 3
              horizontalAlignment: Text.AlignHCenter
              verticalAlignment: Text.AlignBottom
            }

            RowLayout {
              Item {
                id: modIconWrapper
                Layout.preferredWidth: modIcon.implicitWidth
                Layout.fillHeight: true

                Image {
                  id: modIcon
                  anchors.centerIn: parent
                  smooth: false
                  source: root.controller.getModGradeImageSource(modReshapeComponent.modGrade)

                  Image {
                    id: modImage
                    smooth: false
                    source: modReshapeComponent.modSource
                    anchors.centerIn: modIcon
                    anchors.horizontalCenterOffset: isSupremeMod ? 3 : 0
                    anchors.verticalCenterOffset: isSupremeMod ? -2 : 0
                  } // Image

                  Image {
                    id: gradeHighlightImage
                    source: root.controller.getModGradeHighlightImageSource(modReshapeComponent.modGrade)
                    visible: false
                  } // Image
                } // Image

                MouseArea {
                  anchors.fill: parent
                  hoverEnabled: true

                  onClicked: {
                    if (tibiaMouseCursorController) {
                      tibiaMouseCursorController.setPointingHand(false);
                    }
                    root.controller.selectModInFragmentWorkshop(modReshapeComponent.modId, modReshapeComponent.isSupremeMod);
                  }

                  onEntered: {
                    if (tibiaMouseCursorController) {
                      tibiaMouseCursorController.setPointingHand(true);
                    }
                    gradeHighlightImage.visible = true
                  }

                  onExited: {
                    if (tibiaMouseCursorController) {
                      tibiaMouseCursorController.setPointingHand(false);
                    }
                    gradeHighlightImage.visible = false
                  }

                  Tooltip {
                    // needs to be child of mouse are to work there fore more complex positioning
                    width: modImage.width
                    height: modImage.height
                    anchors.centerIn: parent
                    anchors.horizontalCenterOffset: modImage.anchors.horizontalCenterOffset
                    anchors.verticalCenterOffset: modImage.anchors.verticalCenterOffset
                    text: modReshapeComponent.modEffect
                  } // Tooltip
                }
              } // Item

              ColumnLayout {
                TibiaButton {
                  id: reshapeButton
                  text: qsTrId("skill_wheel_atelier_reshape")
                  Layout.fillWidth: true
                  enabled: !goldCostView.tooLowBalance && !fragmentCostView.tooLowBalance && root.isPremium && root.canBeChanged && !root.selectedGem.isLocked

                  Tooltip {
                    anchors.fill: parent
                    text: {
                      if (reshapeButton.enabled) {
                        return "";
                      }
                      let messages = []
                      if (goldCostView.tooLowBalance) {
                        messages.push(
                          qsTrId("skill_wheel_atelier_reshape_insufficient_money")
                            .arg(TextHelper.formatNumberWithThousandSeparators(modReshapeComponent.costGold)))
                      }
                      if (fragmentCostView.tooLowBalance) {
                        messages.push(
                          qsTrId("skill_wheel_atelier_reshape_insufficient_fragments")
                            .arg(TextHelper.formatNumberWithThousandSeparators(modReshapeComponent.costFragments))
                            .arg(isSupremeMod
                              ? qsTrId("skill_wheel_atelier_gem_quality_greater")
                              : qsTrId("skill_wheel_atelier_gem_quality_lesser"))
                            )
                      }
                      if (!root.isPremium) {
                        messages.push(qsTrId("skill_wheel_atelier_reshape_not_premium"))
                      }
                      if (!root.canBeChanged) {
                        messages.push(qsTrId("skill_wheel_atelier_reshape_not_in_temple"))
                      }
                      if (root.selectedGem.isLocked) {
                        messages.push(qsTrId("skill_wheel_atelier_reshape_still_locked"))
                      }
                      return messages.join("\n")
                    }
                  } // Tooltip

                  onClicked: {
                    root.controller.reshapeGem(root.selectedGem.gemId, modReshapeComponent.modIdx);
                  }
                }

                RowLayout {
                  TibiaCurrencyView {
                    id: goldCostView
                    iconId: "GoldCoin"
                    rightAligned: true
                    Layout.preferredWidth: 25
                    Layout.fillWidth: true
                    price: TextHelper.formatNumberWithThousandSeparatorsAndThousandShortcutsAsMultiString(modReshapeComponent.costGold)
                    tooLowBalance: modReshapeComponent.costGold > modReshapeComponent.goldBalance
                  } // TibiaCurrencyView

                  TibiaCurrencyView {
                    id: fragmentCostView
                    iconId: isSupremeMod ? "GreaterFragments" : "LesserFragments"
                    rightAligned: true
                    Layout.preferredWidth: 25
                    price: TextHelper.formatNumberWithThousandSeparatorsAndThousandShortcutsAsMultiString(modReshapeComponent.costFragments)
                    tooLowBalance: modReshapeComponent.costFragments > modReshapeComponent.fragmentBalance
                  } // TibiaCurrencyView
                } // RowLayout
              } // ColumnLayout
            } // RowLayout
          } // ColumnLayout
        } // component ModReshapeComponent

        Item { Layout.fillHeight: true }

        ModReshapeComponent {
          enabled: root.hasSelectedGem && root.selectedGem.quality >= TibiaEnums.EGemQuality.Lesser

          modGrade:  enabled ? root.selectedGem.mod1Grade : 0
          modSource: enabled ? root.selectedGem.mod1ImageSource : ""
          modEffect: enabled ? root.selectedGem.mod1EffectString : ""
          modId:     enabled ? root.selectedGem.mod1Id : 0
          modIdx:    0

          costGold: root.controller.reshapeGemCost(TibiaEnums.EGemQuality.Lesser, true)
          costFragments: root.controller.reshapeGemCost(TibiaEnums.EGemQuality.Lesser, false)
        } // ModReshapeComponent

        Item { Layout.fillHeight: true }

        ModReshapeComponent {
          enabled: root.hasSelectedGem && root.selectedGem.quality >= TibiaEnums.EGemQuality.Regular

          modGrade:  enabled ? root.selectedGem.mod2Grade : 0
          modSource: enabled ? root.selectedGem.mod2ImageSource : ""
          modEffect: enabled ? root.selectedGem.mod2EffectString : ""
          modId:     enabled ? root.selectedGem.mod2Id : 0
          modIdx:    1

          costGold: root.controller.reshapeGemCost(TibiaEnums.EGemQuality.Regular, true)
          costFragments: root.controller.reshapeGemCost(TibiaEnums.EGemQuality.Regular, false)
        } // ModReshapeComponent

        Item { Layout.fillHeight: true }

        ModReshapeComponent {
          enabled: root.hasSelectedGem && root.selectedGem.quality >= TibiaEnums.EGemQuality.Greater

          modGrade:  enabled ? root.selectedGem.mod3Grade : 0
          modSource: enabled ? root.selectedGem.mod3ImageSource : ""
          modEffect: enabled ? root.selectedGem.mod3EffectString : ""
          modId:     enabled ? root.selectedGem.mod3Id : 0
          modIdx:    2

          costGold: root.controller.reshapeGemCost(TibiaEnums.EGemQuality.Greater, true)
          costFragments: root.controller.reshapeGemCost(TibiaEnums.EGemQuality.Greater, false)
        } // ModReshapeComponent

        TibiaButton {
          id: dismantleButton
          Layout.fillWidth: true
          text: qsTrId("skill_wheel_atelier_dismantle")
          enabled: root.hasSelectedGem
            && root.isPremium && root.canBeChanged
            && !root.selectedGem.isSocketed && root.selectedGem.canBeDismantled != undefined && root.selectedGem.canBeDismantled && !root.selectedGem.isLocked

          onClicked: root.controller.dismantleGem(root.selectedGem.gemId)

          Tooltip {
            anchors.fill: parent
            text: {
              if (dismantleButton.enabled || !root.hasSelectedGem) {
                return ""
              }
              let messages = []
              if (!root.isPremium) {
                messages.push(qsTrId("skill_wheel_atelier_dismantle_not_premium"))
              }
              if (!root.canBeChanged) {
                messages.push(qsTrId("skill_wheel_atelier_dismantle_not_in_temple"))
              }
              if (root.selectedGem.isSocketed) {
                messages.push(qsTrId("skill_wheel_atelier_dismantle_still_socketed"))
              } else if (!root.selectedGem.canBeDismantled) {
                messages.push(qsTrId("skill_wheel_atelier_dismantle_changes_not_applied"))
              }
              if (root.selectedGem.isLocked) {
                messages.push(qsTrId("skill_wheel_atelier_dismantle_still_locked"))
              }
              return messages.join("\n")
            }
          } // Tooltip
        } // TibiaButton
      } // ColumnLayout
    } // TibiaFrame2PixelUpFilledWithCaption (Gem Modification)

    // Lower mid panel: List of all identified gems
    ColumnLayout {
      Layout.fillWidth: true
      Layout.fillHeight: true
      spacing: TibiaStyle.marginRelated

      RowLayout {
        id: middleControlsLayout
        Layout.fillWidth: true
        Layout.preferredHeight: TibiaStyle.buttonHeightDefault

        spacing: TibiaStyle.marginRelated
        readonly property int filterControlsWidth: 155

        TibiaTextSearchField {
          id: searchField
          Layout.preferredWidth: middleControlsLayout.filterControlsWidth
          shouldBeText: gemSelectionModel.model.sourceModel.filterModName

          onSearchTextChanged: {
            gemSelectionModel.model.sourceModel.filterModName = searchText
          }

          Keys.onPressed: (event) => {
            if (focus && (event.key == Qt.Key_Enter || event.key == Qt.Key_Return)) {
              searchText = text;
              event.accepted = true;
              //avoid forward to OK-Button which closes the dialogue
            }
          } //Keys.onPressed

        } //TibiaTextSearchField

        TibiaComboBox {
          Layout.preferredWidth: middleControlsLayout.filterControlsWidth
          shouldBeCurrentIndex: gemSelectionModel.model.sourceModel.filterGemQuality + 1
          textRole: "text"
          valueRole: "value"
          model: ListModel {
            id: filterQualityModel
            ListElement { text: qsTrId("skill_wheel_atelier_gem_quality_filter_all");
                          value: -1 }
            ListElement { text: qsTrId("skill_wheel_atelier_gem_quality_filter_lesser");
                          value: TibiaEnums.EGemQuality.Lesser }
            ListElement { text: qsTrId("skill_wheel_atelier_gem_quality_filter_regular");
                          value: TibiaEnums.EGemQuality.Regular }
            ListElement { text: qsTrId("skill_wheel_atelier_gem_quality_filter_greater");
                          value: TibiaEnums.EGemQuality.Greater }
          }

          onActivated: (index) => {
            if (gemSelectionModel.model.sourceModel.filterGemQuality != filterQualityModel.get(index).value) {
              gemSelectionModel.model.sourceModel.filterGemQuality = filterQualityModel.get(index).value
              root.controller.selectVessel(-1)
              gemsGrid.currentIndex = -1
            }
          }
        }

        TibiaCheckBox {
          text: qsTrId("skill_wheel_atelier_gem_show_locked_only")
          shouldBeChecked: gemSelectionModel.model.sourceModel.showLockedOnly
          onCheckedChanged: {
            gemSelectionModel.model.sourceModel.showLockedOnly = checked;
          }
        }

        TibiaButton {
          Layout.leftMargin: TibiaStyle.marginWide - parent.spacing
          imageSource: "/images/icon-arrow.png"
          imageSourceDisabled: "/images/icon-arrow-disabled.png"
          Layout.preferredWidth: TibiaStyle.buttonHeightDefault
          enabled: gemSelectionModel.model.hasPreviousPage

          onClicked: {
            gemSelectionModel.model.currentPage -= 1
          }
        } // TibiaButton

        TibiaText {
          Layout.fillWidth: true
          horizontalAlignment: Text.AlignHCenter
          text: qsTrId("skill_wheel_atelier_page_count_total")
            .arg(gemSelectionModel.model.currentPage)
            .arg(gemSelectionModel.model.numberOfPages)
            .arg(gemSelectionModel.model.sourceModel.rowCount())
            //rowCount has no change signal but currentPage and numberOfPages have so this works
        } // TibiaText

        TibiaButton {
          imageSource: "/images/icon-arrow.png"
          imageSourceDisabled: "/images/icon-arrow-disabled.png"
          Layout.preferredWidth: TibiaStyle.buttonHeightDefault
          imageMirrored: true
          enabled: gemSelectionModel.model.hasNextPage

          onClicked: {
            gemSelectionModel.model.currentPage += 1
          }
        } // TibiaButton
      } // RowLayout

      TibiaFrame1PixelDown {
        Layout.fillWidth: true
        Layout.fillHeight: true

        GridView {
          id: gemsGrid
          anchors.fill: parent
          anchors.margins: parent.borderWidth

          interactive: false
          boundsBehavior: Flickable.StopAtBounds

          readonly property int rowCount: 3
          readonly property int columnCount: 4
          readonly property int spacing: TibiaStyle.marginRelated
          cellWidth: Math.ceil((width / columnCount) - (spacing / columnCount))
          cellHeight: Math.ceil((height / rowCount) - (spacing / rowCount))

          model: gemSelectionModel.model

          delegate: MouseArea {
            id: gridCell

            required property int index
            required property string gemName
            required property int gemImageSource
            required property bool isSocketed
            required property url mod1ImageSource
            required property string mod1EffectString
            required property int mod1Grade
            required property url mod2ImageSource
            required property string mod2EffectString
            required property int mod2Grade
            required property url mod3ImageSource
            required property string mod3EffectString
            required property int mod3Grade
            required property int gemId
            required property bool isLocked
            required property bool canBeDismantled

            width: gemsGrid.cellWidth
            height: gemsGrid.cellHeight

            onClicked: (mouse) => {
              let clickedIndex = gemsGrid.model.index(index, 0)
              if (gemSelectionModel.currentIndex == clickedIndex) {
                gemSelectionModel.clearCurrentIndex()
                gemsGrid.currentIndex = -1
                root.controller.selectVessel(-1)
              } else {
                gemSelectionModel.setCurrentIndex(clickedIndex, ItemSelectionModel.ClearAndSelect)
                gemsGrid.currentIndex = index
                let vesselIndex = root.vesselIndexForGemId(gridCell.gemId)
                if (vesselIndex >= 0) {
                  root.controller.selectVessel(vesselIndex)
                }
              }

              releaseFocusFromSearchField();
            }

            TibiaFrame2PixelUpFilled {
              width: gemsGrid.cellWidth - gemsGrid.spacing
              height: gemsGrid.cellHeight - gemsGrid.spacing
              anchors { bottom: parent.bottom; right: parent.right; }

              Rectangle {
                anchors.fill: parent
                anchors.margins: -3
                color: "transparent"
                border { color: "white"; width: 2 }
                visible: gemSelectionModel.currentIndex == gemsGrid.model.index(gridCell.index, 0)
              } // selection border

              Item {
                width: lockButton.width
                height: lockButton.height
                anchors { margins: parent.marginsToContent + TibiaStyle.marginRelated;
                            left: parent.left; top: parent.top }

                TibiaIconButton {
                  id: lockButton
                  checkable: true
                  checked: gridCell.isLocked
                  opacity: root.canBeChanged ? 1.0 : 0.6
                  enabled: root.canBeChanged
                  sourceDown: "/images/icon-locked.png"
                  sourceUp: "/images/icon-unlocked.png"
                  anchors.fill: parent
                  onClicked: {
                    root.controller.toggleGemLock(gridCell.gemId);
                  }

                } // TibiaIconButton

                Tooltip {
                  anchors.fill: parent
                  text: {
                    if (lockButton.enabled) {
                      return qsTrId("skill_wheel_atelier_lock_button_tooltip");
                    } else {
                      let messages = [];
                      if (!root.canBeChanged) {
                        messages.push(qsTrId("skill_wheel_atelier_lock_not_in_temple"));
                      }
                      return messages.join("\n");
                    }
                  }

                } // Tooltip

              } // Item

              SingleObjectAppearanceInstanceRenderer {
                typeid: gridCell.gemImageSource
                width: TibiaStyle.mapWindowPixelPerField
                height: width
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: TibiaStyle.marginUnrelated
              } // SingleObjectAppearanceInstanceRenderer

              Image {
                anchors { margins: parent.marginsToContent + TibiaStyle.marginNarrow;
                          right: parent.right; top: parent.top }
                source: isSocketed ? "/images/skillwheel/icon-gem-socketed.png" : ""
              } // Image

              RowLayout {
                anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter;
                          margins: parent.marginsToContent }

                spacing: TibiaStyle.marginNarrow

                Image {
                  source: root.controller.getModGradeImageSource(gridCell.mod1Grade)

                  Image {
                    source: gridCell.mod1ImageSource
                    anchors.centerIn: parent

                    Tooltip {
                      anchors.fill: parent
                      text: gridCell.mod1EffectString
                    }
                  }
                } // Image

                Image {
                  source: root.controller.getModGradeImageSource(gridCell.mod2Grade)
                  visible: gridCell.mod2ImageSource != ""

                  Image {
                    source: gridCell.mod2ImageSource
                    anchors.centerIn: parent

                    Tooltip {
                      anchors.fill: parent
                      text: gridCell.mod2EffectString
                    }
                  }
                } // Image

                Image {
                  source: root.controller.getModGradeImageSource(gridCell.mod3Grade)
                  visible: gridCell.mod3ImageSource != ""

                  Image {
                    source: gridCell.mod3ImageSource
                    anchors.centerIn: parent
                    anchors.horizontalCenterOffset: 3
                    anchors.verticalCenterOffset: -2

                    Tooltip {
                      anchors.fill: parent
                      text: gridCell.mod3EffectString
                    }
                  }
                } // Image
              } /// RowLayout
            } // TibiaFrame2PixelUpFilled
          } // MouseArea
        } // GridView
      } // TibiaFrame1PixelDown
    } // ColumnLayout (gem inventory)

    // Lower right column: vessel gem actions, then reveal unidentified gems
    TibiaFrame2PixelUpFilledWithCaption {
      caption: qsTrId("skill_wheel_atelier_relevation_caption")
      Layout.preferredWidth: root.leftColumnWidth
      Layout.fillHeight: true

      component GemRelevation: ColumnLayout {
        id: relevationComponent
        property int appearanceTypeId: 0
        property string gemName
        property int gemCount: 0
        property real revealCost: 0
        property real bankBalance: 0
        property int gemQuality
        property int totalRevealedGems: 0

        Item {
          Layout.fillWidth: true
          Layout.fillHeight: true

          SingleObjectAppearanceInstanceRenderer {
            typeid: relevationComponent.appearanceTypeId
            width: TibiaStyle.mapWindowPixelPerField
            height: width
            anchors.centerIn: parent

            Tooltip {
              anchors.fill: parent
              text: relevationComponent.gemName
            } // ToolTip
          } // SingleObjectAppearanceInstanceRenderer
        } // Item

        TibiaText {
          text: qsTrId("skill_wheel_atelier_reveal_name_count").arg(relevationComponent.gemName).arg(relevationComponent.gemCount)
          Layout.fillWidth: true
          Layout.minimumHeight: TibiaStyle.defaultTextLineHeight * 2
          horizontalAlignment: Text.AlignHCenter
          verticalAlignment: Text.AlignBottom
          wrapMode: Text.Wrap
        }

        RowLayout {
          spacing: TibiaStyle.marginNarrow

          Item {
            // Workaround for tooltip + disabled element
            width: revealButton.width
            height: revealButton.height

            TibiaButton {
              id: revealButton
              text: qsTrId("skill_wheel_atelier_reveal")
              width: TibiaStyle.buttonWidthBroad
              enabled: !revealCostView.tooLowBalance && relevationComponent.gemCount > 0 && relevationComponent.totalRevealedGems < root.maxRevealedGems && root.isPremium && root.canBeChanged

              onClicked: {
                root.controller.revealGem(relevationComponent.gemQuality);
              }
            } // TibiaButton

            Tooltip {
              anchors.fill: parent
              text: {
                if (revealButton.enabled) {
                  return "";
                }
                let messages = []
                if (revealCostView.tooLowBalance) {
                  messages.push(
                    qsTrId("skill_wheel_atelier_reveal_insufficient_money")
                      .arg(TextHelper.formatNumberWithThousandSeparators(relevationComponent.revealCost)))
                }
                if (relevationComponent.gemCount == 0) {
                  messages.push(qsTrId("skill_wheel_atelier_reveal_no_unrevealed_gems"))
                }
                if (relevationComponent.totalRevealedGems >= root.maxRevealedGems) {
                  messages.push(
                    qsTrId("skill_wheel_atelier_reveal_too_many_gems")
                      .arg(root.maxRevealedGems))
                }
                if (!root.isPremium) {
                  messages.push(qsTrId("skill_wheel_atelier_reveal_not_premium"))
                }
                if (!root.canBeChanged) {
                  messages.push(qsTrId("skill_wheel_atelier_reveal_not_in_temple"))
                }
                return messages.join("\n")
              }
            } // Tooltip
          } // Item

          TibiaCurrencyView {
            id: revealCostView
            Layout.fillWidth: true
            Layout.minimumWidth: TibiaStyle.currencyViewWidthShort
            iconId: "GoldCoin"
            rightAligned: true
            price: TextHelper.formatNumberWithThousandSeparatorsAndThousandShortcutsAsMultiString(relevationComponent.revealCost)
            tooLowBalance: relevationComponent.revealCost > relevationComponent.bankBalance
          }
        }
      } // component GemRelevation

      TibiaGuiHelp {
        anchors { right: parent.right; top: parent.top;
                  topMargin: parent.borderWidth + 1;
                  rightMargin: parent.borderWidth + TibiaStyle.marginNarrow; }
        text: qsTrId("skill_wheel_atelier_reveal_tooltip")
      }

      ColumnLayout {
        anchors.fill: parent
        anchors.margins: parent.marginsToContent
        anchors.topMargin: parent.topMarginToContent

        GemRelevation {
          Layout.fillWidth: true
          Layout.fillHeight: true

          appearanceTypeId: root.controller.unrevealedLesserGemAppearanceType
          gemName: root.controller.unrevealedLesserGemName
          gemCount: root.controller.unrevealedLesserGems
          revealCost: root.controller.revealLesserGemCost
          bankBalance: storeAndResourceBalanceHelper.totalGoldBalance
          gemQuality: TibiaEnums.EGemQuality.Lesser
          totalRevealedGems: root.controller.gemsInInventory
        }

        GemRelevation {
          Layout.fillWidth: true
          Layout.fillHeight: true

          appearanceTypeId: root.controller.unrevealedRegularGemAppearanceType
          gemName: root.controller.unrevealedRegularGemName
          gemCount: root.controller.unrevealedRegularGems
          revealCost: root.controller.revealRegularGemCost
          bankBalance: storeAndResourceBalanceHelper.totalGoldBalance
          gemQuality: TibiaEnums.EGemQuality.Regular
          totalRevealedGems: root.controller.gemsInInventory
        }

        GemRelevation {
          Layout.fillWidth: true
          Layout.fillHeight: true

          appearanceTypeId: root.controller.unrevealedGreaterGemAppearanceType
          gemName: root.controller.unrevealedGreaterGemName
          gemCount: root.controller.unrevealedGreaterGems
          revealCost: root.controller.revealGreaterGemCost
          bankBalance: storeAndResourceBalanceHelper.totalGoldBalance
          gemQuality: TibiaEnums.EGemQuality.Greater
          totalRevealedGems: root.controller.gemsInInventory
        }
      }
    } // TibiaFrame2PixelUpFilledWithCaption (Gem Relevation)
  } // RowLayout
} // ColumnLayout
