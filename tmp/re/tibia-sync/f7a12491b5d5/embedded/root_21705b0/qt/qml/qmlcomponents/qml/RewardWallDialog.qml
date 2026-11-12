import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import qmlcomponents
import QtQuick.LegacyControls

TibiaDialog {
  id: dialogRoot
  caption: qsTrId("rewardwall_caption")
  width: 800 + 2 * TibiaStyle.dialogMarginBorder

  required property var controller

  property bool isPremium: controller != null && controller.isPremium

  onReturnPressedFunction: function () {}
  onCancelPressedFunction: closeDialog

  function closeDialog() {
    if (controller != null) {
      controller.closeButtonClicked();
    }
  } //function closeDialog

  initialFocusItem: dialogRoot
  KeyNavigation.tab: dialogRoot
  KeyNavigation.backtab: dialogRoot

  ColumnLayout {
    anchors {
      left: parent.left
      top: parent.top
      right: parent.right
    }
    spacing: TibiaStyle.marginRelated

    RowLayout {
      id: contentLayout
      Layout.fillWidth: true
      Layout.fillHeight: true
      spacing: TibiaStyle.marginUnrelated

      TibiaFrame2PixelUpFilledWithCaption {
        id: restingAreaBonusesFrame
        caption: qsTrId("rewardwall_resting_area_bonuses_header")
        Layout.preferredWidth: 248
        Layout.preferredHeight: dailyRewards.height

        ColumnLayout {
          id: restingBonusLayout
          anchors.top: parent.top
          anchors.left: parent.left
          anchors.right: parent.right
          anchors.bottom: parent.bottom
          anchors.topMargin: parent.captionHeight

          spacing: TibiaStyle.marginRelated

          RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: TibiaStyle.marginWide

            Item {
              Layout.fillWidth: true
            }

            RowLayout {
              spacing: TibiaStyle.marginRelated

              Item {
                Layout.preferredWidth: TibiaStyle.rewardWallItemSize
                Layout.preferredHeight: TibiaStyle.rewardWallItemSize

                Image {
                  anchors.centerIn: parent
                  source: {
                    if (controller != null) {
                      if (controller.rewardStreakLength >= 100) {
                        return "/images/dailyreward/icon-rewardstreak-gold.png";
                      } else if (controller.rewardStreakLength >= 50) {
                        return "/images/dailyreward/icon-rewardstreak-silver.png";
                      } else if (controller.rewardStreakLength >= 25) {
                        return "/images/dailyreward/icon-rewardstreak-bronze.png";
                      }
                    }
                    return "/images/dailyreward/icon-rewardstreak-default.png";
                  } //source

                  TibiaText {
                    x: 16
                    y: 15
                    width: 34
                    height: 9
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                    text: controller != null ? controller.rewardStreakLength : "-"
                  } //TibiaText

                  MouseArea {
                    id: rewardStreakLengthHoverArea
                    anchors.fill: parent
                    z: -1
                    hoverEnabled: dialogRoot.visible
                  } //MouseArea
                } //Image
              }

              ColumnLayout {

                RowLayout {

                  TibiaProgressBar {
                    id: timeLeftKeepRewardStreak
                    Layout.preferredWidth: TibiaStyle.rewardWallItemSize
                    Layout.preferredHeight: TibiaStyle.progressBarLargeHeight

                    property bool isRunning: controller != null && controller.dailyRewardStreakState == TibiaEnums.Running
                    fillPercentage: controller != null ? controller.rewardStreakTimeLeftProgress : 0.0

                    frameSource: "/images/1pixel-down-frame.png"
                    backgroundSource: "/images/backdrop-dark-grey.png"
                    fillSource: isRunning ? "/images/progressbar-orange-large.png" : "/images/progressbar-grey-large.png"

                    frameBorder {
                      left: 1
                      right: 1
                      top: 1
                      bottom: 1
                    }
                    fillOffset {
                      left: 1
                      right: 1
                      top: 1
                      bottom: 1
                    }

                    TibiaText {
                      anchors {
                        left: parent.left
                        right: parent.right
                        verticalCenter: parent.verticalCenter
                      }
                      horizontalAlignment: Text.AlignHCenter
                      styleType: "White"

                      text: controller != null ? controller.rewardStreakTimeLeft : "00:00"
                      visible: text != ""
                    } //TibiaText

                    Image {
                      anchors.centerIn: parent
                      source: {
                        if (controller != null) {
                          if (controller.dailyRewardStreakState == TibiaEnums.AlreadyCollected || controller.dailyRewardStreakState == TibiaEnums.NeverCollected) {
                            return "/images/dailyreward/icon-checkmark.png";
                          } else if (controller.dailyRewardStreakState == TibiaEnums.Frozen) {
                            return "/images/icon-lock-red.png";
                          }
                        }
                        return "";
                      }

                      visible: source != ""
                    }

                    MouseArea {
                      id: rewardStreakExpiringHoverArea
                      anchors.fill: parent
                      z: -1
                      hoverEnabled: dialogRoot.visible
                    }
                  }

                  TibiaGuiHelp {
                    useRichText: true
                    text: controller.rewardStreakStateInfoText
                  }
                }

                RowLayout {

                  TibiaCurrencyView {
                    Layout.preferredWidth: timeLeftKeepRewardStreak.width
                    Layout.alignment: Qt.AlignTop

                    iconId: "DailyRewardJoker"
                    price: controller != null ? controller.jokersNeededForCollecting <= controller.maxDailyRewardJokerAmount ? controller.jokersNeededForCollecting : ">%1".arg(controller.maxDailyRewardJokerAmount) : "-"
                    tooLowBalance: controller != null && controller.dailyRewardJokerAmount < controller.jokersNeededForCollecting

                    MouseArea {
                      id: jokersNeededCurrencyViewTextHoverArea
                      anchors.fill: parent
                      z: -1
                      hoverEnabled: dialogRoot.visible
                    } //MouseArea
                  } //TibiaCurrencyView

                  TibiaGuiHelp {
                    useRichText: true
                    text: controller.jokersNeededCurrencyViewhInfoText()
                  }
                } // RowLayout
              } // Columnlayout
            }

            Item {
              Layout.fillWidth: true
            }
          }

          TibiaHorizontalSeparator {
            Layout.margins: TibiaStyle.marginUnrelated
          }

          RowLayout {
            Layout.leftMargin: TibiaStyle.marginUnrelated
            Layout.rightMargin: TibiaStyle.marginUnrelated
            Layout.fillWidth: true

            TibiaText {
              Layout.fillWidth: true
              verticalAlignment: Text.AlignVCenter
              wrapMode: Text.Wrap

              text: controller.rewardStreakInfoText
            } // TibiaText

            TibiaGuiHelp {
              useRichText: true
              text: qsTrId("rewardwall_reward_streak")
            } // TibiaGuiHelp
          } // RowLayout

          Item {
            Layout.fillHeight: true
          }
        }

        // Rectangle {
        //   anchors.top: restingBonusLayout.top
        //   anchors.left: restingBonusLayout.left
        //   anchors.right: restingBonusLayout.right
        //   anchors.bottom: restingBonusLayout.bottom

        //   color: "transparent"
        //   border.color: "red"
        //   border.width: 1
        // }
      }

      Image {
        Layout.alignment: Qt.AlignVCenter
        source: "/images/icon-arrow-rightlarge.png"
      }

      TibiaFrame2PixelUpFilledWithCaption {
        id: dailyRewards
        caption: qsTrId("rewardwall_daily_rewards_header")
        Layout.fillWidth: true
        Layout.preferredHeight: dailyRewardLayout.height + marginsToContent + topMarginToContent

        ColumnLayout {
          id: dailyRewardLayout
          anchors.top: parent.top
          anchors.left: parent.left
          anchors.right: parent.right
          anchors.margins: parent.marginsToContent
          anchors.topMargin: parent.topMarginToContent
          spacing: 0

          RowLayout {
            spacing: 0
            Layout.alignment : Qt.AlignHCenter

            Repeater {
              id: dailyRewardRepeater
              model: controller != null ? controller.dailyRewards : null

              property int hoveredRewardIndex: -1
              property int hoveredRewardStateIndex: -1
              property QtObject currentHoverRewardIndexMouseArea
              property QtObject currentHoverRewardStateIndexMouseArea

              Item {
                property bool isLastElement: index == (dailyRewardRepeater.count - 1)
                property bool isCurrentReward: controller != null && (index == controller.nextDailyRewardPosition)
                property bool isCollected: controller != null && (index < controller.nextDailyRewardPosition)
                property bool readyToPickedUp: isCurrentReward && (controller != null) && !controller.isDailyRewardLocked

                Layout.preferredWidth: TibiaStyle.rewardWallItemSize + (isLastElement ? 0 : TibiaStyle.marginRelated)
                Layout.preferredHeight: TibiaStyle.rewardWallItemSize + TibiaStyle.marginRelated + TibiaStyle.progressBarLargeHeight

                Item {
                  id: rewardFrame
                  width: TibiaStyle.rewardWallItemSize
                  height: TibiaStyle.rewardWallItemSize

                  TibiaFrame1PixelDown {
                    anchors.fill: parent
                    visible: !readyToPickedUp

                    Image {
                      id: rewardIconBackground
                      anchors.fill: parent
                      source: "/images/dailyreward/reward-scroll-backdrop.png"
                    }

                    Image {
                      id: rewardIcon
                      anchors.centerIn: parent
                      anchors.horizontalCenterOffset: -5
                      anchors.verticalCenterOffset: -5
                      source: controller != null ? controller.dailyRewardIcon(index, dialogRoot.isPremium) : ""
                    }
                    
                    Image {
                      anchors.left: parent.left
                      anchors.top: parent.top
                      anchors.leftMargin: 1
                      anchors.topMargin: 1
                      source: "/images/dailyreward/icon-reward-premium-banner.png"
                      visible : controller.isPremium
                    } //Image

                    TibiaText {
                      color: "#35210d"
                      anchors.left: parent.left
                      anchors.right: parent.right
                      anchors.bottom: parent.bottom
                      anchors.bottomMargin: parent.down ? 15 : 14
                      horizontalAlignment: Text.AlignHCenter
                      text: controller.dailyRewardNumbersText(index, dialogRoot.isPremium)
                    }
                    
                  } //TibiaFrame1PixelDown

                  TibiaButton {
                    anchors.fill: parent
                    visible: readyToPickedUp
                    imageSource: "/images/dailyreward/reward-scroll-backdrop.png"
                    onClicked: controller != null ? controller.collectRewardButtonClicked() : undefined

                    Image {
                      anchors.left: parent.left
                      anchors.top: parent.top
                      anchors.leftMargin: parent.down ? 2 : 1
                      anchors.topMargin: parent.down ? 2 : 1
                      source: "/images/dailyreward/icon-reward-premium-banner.png"
                      visible : controller.isPremium
                    } 

                    Image {
                      anchors.centerIn: parent
                      anchors.horizontalCenterOffset: parent.down ? -4 : -5
                      anchors.verticalCenterOffset: parent.down ? -4 : -5                     

                      source: controller != null ? controller.dailyRewardIcon(index, dialogRoot.isPremium) : ""
                    }

                    TibiaText {
                      color: "#35210d"
                      anchors.left: parent.left
                      anchors.right: parent.right
                      anchors.bottom: parent.bottom
                      anchors.leftMargin: parent.down ? 0 : -2
                      anchors.bottomMargin: parent.down ? 13 : 14
                      horizontalAlignment: Text.AlignHCenter
                      text: controller.dailyRewardNumbersText(index, dialogRoot.isPremium)
                    }
                  } //TibiaButton

                  TibiaDisabledOverlay {
                    anchors.fill: parent
                    anchors.margins: 1
                    visible: !isCurrentReward
                  } //TibiaDisabledOverlay

                  MouseArea {
                    id: dailyRewardHoverArea
                    anchors.fill: parent
                    hoverEnabled: dialogRoot.visible
                    onEntered: {
                      dailyRewardRepeater.currentHoverRewardIndexMouseArea = this;
                      dailyRewardRepeater.hoveredRewardIndex = index;
                    }
                    onExited: {
                      if (dailyRewardRepeater.currentHoverRewardIndexMouseArea === this) {
                        dailyRewardRepeater.hoveredRewardIndex = -1;
                      }
                    }

                    propagateComposedEvents: true
                    onClicked: mouse => mouse.accepted = false
                    onPressed: mouse => mouse.accepted = false
                    onReleased: mouse => mouse.accepted = false
                  } //MouseArea
                } //Item

                Image {
                  anchors.left: rewardFrame.right
                  anchors.verticalCenter: rewardFrame.verticalCenter
                  source: isCollected ? "/images/dailyreward/icon-rewardarrow-active.png" : "/images/dailyreward/icon-rewardarrow-inactive.png"
                  visible: !isLastElement
                } //Image

                Item {
                  anchors.left: parent.left
                  anchors.top: rewardFrame.bottom
                  anchors.topMargin: TibiaStyle.marginRelated
                  width: rewardFrame.width
                  height: TibiaStyle.progressBarLargeHeight

                  TibiaFrame1PixelDown {
                    anchors.fill: parent
                    visible: isCollected

                    BorderImage {
                      anchors.fill: parent
                      anchors.margins: parent.borderWidth
                      source: "/images/backdrop-dark-grey.png"
                      horizontalTileMode: BorderImage.Repeat
                      verticalTileMode: BorderImage.Repeat
                    } //Image

                    Image {
                      anchors.centerIn: parent
                      source: "/images/dailyreward/icon-checkmark.png"
                    } //Image
                  } //TibiaFrame1PixelDown

                  TibiaProgressBar {
                    id: timeUntillPickUp
                    anchors.fill: parent
                    visible: !isCollected && !readyToPickedUp

                    property bool isRunning: controller != null && controller.isDailyRewardLocked && isCurrentReward
                    fillPercentage: controller != null ? (isRunning ? controller.dailyRewardTimeToUnlockProgress : 1.0) : 0.0

                    frameSource: "/images/1pixel-down-frame.png"
                    backgroundSource: "/images/backdrop-dark-grey.png"
                    fillSource: isRunning ? "/images/progressbar-orange-large.png" : "/images/progressbar-grey-large.png"

                    frameBorder {
                      left: 1
                      right: 1
                      top: 1
                      bottom: 1
                    }
                    fillOffset {
                      left: 1
                      right: 1
                      top: 1
                      bottom: 1
                    }

                    TibiaText {
                      anchors {
                        left: parent.left
                        right: parent.right
                        verticalCenter: parent.verticalCenter
                      }
                      horizontalAlignment: Text.AlignHCenter
                      styleType: "White"

                      text: controller != null ? controller.dailyRewardTimeToUnlock : "00:00"
                      visible: parent.isRunning
                    } //TibiaText

                    Image {
                      anchors.centerIn: parent
                      source: "/images/icon-lock-red.png"

                      visible: !parent.isRunning
                    } //Image

                  } //TibiaProgressBar

                  TibiaButton {
                    anchors.fill: parent
                    visible: readyToPickedUp
                    text: qsTrId("rewardwall_claim_button")
                    onClicked: controller != null ? controller.collectRewardButtonClicked() : undefined
                  } //TibiaButton

                  MouseArea {
                    id: dailyRewardStateHoverArea
                    anchors.fill: parent
                    hoverEnabled: dialogRoot.visible
                    onEntered: {
                      dailyRewardRepeater.currentHoverRewardStateIndexMouseArea = this;
                      dailyRewardRepeater.hoveredRewardStateIndex = index;
                    }
                    onExited: {
                      if (dailyRewardRepeater.currentHoverRewardStateIndexMouseArea === this) {
                        dailyRewardRepeater.hoveredRewardStateIndex = -1;
                      }
                    }
                    // Hover-only MouseArea: don't consume press/release/click,
                    // let them fall through to the TibiaButton beneath it.
                    propagateComposedEvents: true
                    onClicked: mouse => mouse.accepted = false
                    onPressed: mouse => mouse.accepted = false
                    onReleased: mouse => mouse.accepted = false
                  } //MouseArea
                } //Item
              } //Item
            }
          } //RowLayout

          Image {
            Layout.alignment: Qt.AlignHCenter
            source: controller != null && controller.rewardStreakLength != 0 //very first reward
            && controller.nextDailyRewardPosition == 0 ? "/images/dailyreward/icon-rewardarrow-loop-active.png" : "/images/dailyreward/icon-rewardarrow-loop-inactive.png"
          } //Image

          RowLayout {

            Item {
              Layout.fillWidth: true
            }

            RowLayout {

              Layout.preferredWidth: dailyRewardRepeater.parent.width

              ColumnLayout {
                Layout.margins: TibiaStyle.marginUnrelated
                Layout.fillHeight : true

                TibiaPremiumStateButton {
                  id: premiumButton
                  hasPremium: controller.isPremium
                  onClicked: controller ? controller.getPremiumClicked() : undefined
                }

                TibiaText {
                  Layout.preferredWidth: premiumButton.width
                  wrapMode: Text.Wrap
                  horizontalAlignment: Text.AlignLeft
                  text: dialogRoot.isPremium ? qsTrId("rewardwall_premium_rewards_active") : qsTrId("rewardwall_get_better_rewards_with_premium")
                }
              }

              TibiaVerticalSeparator {
                Layout.topMargin: TibiaStyle.marginUnrelated
                Layout.fillHeight: true
              }

              ColumnLayout {
                Layout.margins: TibiaStyle.marginUnrelated
                Layout.fillWidth: true

                TibiaText {
                  Layout.fillWidth: true

                  wrapMode: Text.Wrap
                  textFormat: Text.RichText
                  text: dailyRewardRepeater.hoveredRewardStateIndex != -1
                        ? controller.dailyRewardStateHoverText(dailyRewardRepeater.hoveredRewardStateIndex)
                        : controller.dailyRewardHoverText(dailyRewardRepeater.hoveredRewardIndex)
                }

                Item {
                  Layout.fillWidth: true
                  Layout.fillHeight: true
                }
              }
            }

            Item {
              Layout.fillWidth: true
            }
          }
        }
      }
    }

    TibiaFrame2PixelUpFilledWithCaption {
      id: historyFrame
      caption: qsTrId("history")
      Layout.fillWidth: true
      Layout.preferredHeight: contentLayout.height
      visible: false

      TibiaTableView {
        id: historyTableView
        anchors {
          left: parent.left
          leftMargin: TibiaStyle.marginRelated
          right: parent.right
          rightMargin: TibiaStyle.marginRelated
          top: parent.top
          topMargin: historyFrame.captionHeight + TibiaStyle.marginRelated
          bottom: parent.bottom
          bottomMargin: TibiaStyle.marginRelated
        }

        model: controller != null ? controller.history : null
        headerVisible: true
        verticalScrollBarPolicy: ScrollBar.AlwaysOn
        horizontalScrollBarPolicy: ScrollBar.AsNeeded

        rowDelegate: Rectangle {
          height: TibiaStyle.tableViewRowHeight2Lines
          color: styleData.row == undefined ? TibiaStyle.tableViewBackgroundColor : styleData.selected ? TibiaStyle.tableViewSelectionColor : styleData.alternate ? TibiaStyle.tableViewAlternateBackgroundColor : TibiaStyle.tableViewItemBackgroundColor
        } // rowDelegate

        TableViewColumn {
          id: timestampColumn
          role: "timestamp"
          title: qsTrId("date_column_header")
          movable: false
          resizable: false
          width: 150
        } //TableViewColumn

        TableViewColumn {
          id: rewardStreakColumn
          role: "newRewardStreakLength"
          title: qsTrId("rewardwall_history_reward_streak_header")
          movable: false
          resizable: false
          width: 50

          delegate: Item {
            anchors.fill: parent

            TibiaText {
              text: styleData.value
              color: modelData != null && modelData.changedBySupport ? TibiaStyle.green1 : styleData.selected ? TibiaStyle.textFieldSelectionTextColor : TibiaStyle.textFieldTextColor
              anchors.fill: parent
              anchors.leftMargin: TibiaStyle.marginNarrow
              anchors.rightMargin: TibiaStyle.marginRelated
              anchors.verticalCenter: parent.verticalCenter
              elide: styleData.elideMode
              horizontalAlignment: Text.AlignRight

              Tooltip {
                anchors.fill: parent
                text: qsTrId("rewardwall_history_changed_by_support_tooltip")
                enabled: modelData != null && modelData.changedBySupport
              } //Tooltip
            } //TibiaText

            TibiaVerticalSeparator {
              anchors {
                top: parent.top
                bottom: parent.bottom
                right: parent.right
              }
            } // TibiaVerticalSeparator
          } //delegate: Item
        } //TableViewColumn

        TableViewColumn {
          role: "description"
          title: qsTrId("rewardwall_history_event_header")
          movable: false
          resizable: false
          width: historyTableView.width - timestampColumn.width - rewardStreakColumn.width - TibiaStyle.scrollBarWidth - 2

          delegate: TibiaText {
            anchors.fill: parent
            anchors.leftMargin: TibiaStyle.marginNarrow
            anchors.rightMargin: TibiaStyle.marginRelated

            text: styleData.value
            color: modelData != null && modelData.changedBySupport ? TibiaStyle.green1 : styleData.selected ? TibiaStyle.textFieldSelectionTextColor : TibiaStyle.textFieldTextColor

            elide: styleData.elideMode
            wrapMode: Text.Wrap
            tooltipMaxWidth: TibiaStyle.tooltipRestrictedWidth
          } //delegate: TibiaText
        } //TableViewColumn
      } //TibiaTableView
    } //TibiaFrame2PixelUpFilledWithCaption

    TibiaHorizontalSeparator {
      Layout.topMargin: TibiaStyle.marginRelated
      Layout.fillWidth: true
    } //TibiaHorizontalSeparator

    RowLayout {
      Layout.alignment: Qt.AlignRight
      spacing: TibiaStyle.marginRelated

      TibiaCurrencyView {
        Layout.preferredWidth: TibiaStyle.currencyViewWidthShort
        rightAligned: true
        balance: controller != null ? controller.cosmeticTokenCount : "-"
        iconId: "CosmeticToken"
      } //TibiaCurrencyView

      TibiaCurrencyView {
        Layout.preferredWidth: TibiaStyle.currencyViewWidthShort
        rightAligned: true
        balance: controller != null ? controller.bountyTaskRerollTokenCount : "-"
        iconId: "TaskReroll"
      } //TibiaCurrencyView

      TibiaCurrencyView {
        Layout.preferredWidth: TibiaStyle.currencyViewWidthShort
        rightAligned: true
        balance: controller != null ? controller.dailyRewardJokerAmount : "-"
        iconId: "DailyRewardJoker"

        MouseArea {
          id: jokersAmountCurrencyViewTextHoverArea
          anchors.fill: parent
          z: -1
          hoverEnabled: dialogRoot.visible
        } //MouseArea
      } //TibiaCurrencyView

      RowLayout {
        spacing: TibiaStyle.marginNarrow

        TibiaCurrencyView {
          Layout.preferredWidth: TibiaStyle.currencyViewWidthShort
          rightAligned: true
          balance: controller != null ? controller.instantRewardAccessAmount : "-"
          iconId: "InstantRewardAccess"
        }//TibiaCurrencyView

        TibiaButton {
          Layout.preferredHeight: closeButton.height
          Layout.preferredWidth: TibiaStyle.buttonWidthStoreSmall
          color: "blue"
          imageSource: "/images/icon-store-16x10.png"
          tooltipText: qsTrId("rewardwall_get_more_instant_reward_access_tooltip")

          onClicked: controller ? controller.getMoreInstantRewardAccessClicked() : undefined
        }//TibiaButton
      } //RowLayout

      Item {
        Layout.fillWidth: true
      }//Item

      TibiaButton {
        text: historyFrame.visible ? qsTrId("back") : qsTrId("history")

        onClicked: {
          // The order in which elements are made invisible/visible is important:
          // Unless elements are made invisible first, the dialog height temporarily increases,
          // which leads to an undesired repositioning of the dialog in y direction
          if (!historyFrame.visible && controller != null) {
            controller.historyButtonClicked();
            contentLayout.visible = false;
            historyFrame.visible = true;
          } else {
            historyFrame.visible = false;
            contentLayout.visible = true;
          }
        } //onClicked
      } //TibiaButton

      TibiaButton {
        id: closeButton
        text: qsTrId("close")

        onClicked: dialogRoot.closeDialog()
      } // TibiaButton
    } // RowLayout
  } // ColumnLayout
} // TibiaDialog
