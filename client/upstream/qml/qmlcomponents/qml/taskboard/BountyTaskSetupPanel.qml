import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtWebChannel
import QtWebEngine

import qmlcomponents
import "qrc:/qt/qml/qmlcomponents/qml/"
import QtQuick.LegacyControls

TibiaFrame2PixelUpFilledWithCaption {
  id: root

  property int rerollTaskCost: 1
  required property int bountyTaskDifficulty
  required property var selectBountyTaskDifficulty
  required property int maxBountyTaskDifficulty
  required property int adeptLevelRequirement
  required property int expertLevelRequirement
  required property int masterLevelRequirement

  required property bool isElite
  required property var setEliteAction

  required property var showPreferredList

  required property bool canRerollTasks
  required property var rerollTasksAction
  required property bool limitOfRerollTokensReached

  
  caption: qsTrId("taskboard_setup_and_reroll")

  RowLayout {
    id: content
    anchors.top: parent.top
    anchors.topMargin: root.captionHeight
    anchors.left: parent.left
    anchors.leftMargin: TibiaStyle.marginUnrelated
    anchors.right: parent.right
    anchors.rightMargin: TibiaStyle.marginUnrelated

    TibiaGuiHelp {
      Layout.alignment: Qt.AlignVCenter
      Layout.leftMargin: TibiaStyle.marginWide
      Layout.rightMargin: TibiaStyle.marginUnrelated
      text: qsTrId("taskboard_task_difficulty_tooltip")
    }

    TibiaVerticalSeparator {
      Layout.margins: TibiaStyle.marginUnrelated
      Layout.preferredHeight: 20
    }

    Item {
      Layout.fillWidth: true
    }

    TibiaText {
      text: qsTrId("taskboard_task_difficulty")
      Layout.topMargin: TibiaStyle.marginUnrelated
      Layout.bottomMargin: TibiaStyle.marginUnrelated
      Layout.leftMargin: TibiaStyle.marginUnrelated
      Layout.rightMargin: TibiaStyle.marginRelated
      Layout.alignment: Qt.AlignVCenter

      Tooltip {
        anchors.fill: parent
        text: qsTrId("taskboard_task_difficulty_label_tooltip")
        maxWidth: TibiaStyle.tooltipRestrictedWidth
      } //Tooltip
    }

    TibiaComboBox {
      id: bountyTaskDifficultyDropdown
      Layout.topMargin: TibiaStyle.marginUnrelated
      Layout.bottomMargin: TibiaStyle.marginUnrelated
      Layout.rightMargin: TibiaStyle.marginUnrelated
      Layout.preferredWidth: TibiaStyle.buttonWidthWider

      model: [
        qsTrId("taskboard_difficulty_beginner"),
        qsTrId("taskboard_difficulty_adept"),
        qsTrId("taskboard_difficulty_expert"),
        qsTrId("taskboard_difficulty_master")
      ]
      
      currentIndex: bountyTaskDifficulty

      // Difficulties above the player's level are greyed out, analogous to the weekly task difficulty buttons.
      isItemEnabled: function (index) {
        return index <= root.maxBountyTaskDifficulty;
      }
      itemTooltipText: function (index) {
        switch (index) {
        case 0: return qsTrId("taskboard_difficulty_beginner_tooltip");
        case 1: return qsTrId("taskboard_difficulty_adept_tooltip").arg(TextHelper.formatNumberWithThousandSeparators(root.adeptLevelRequirement));
        case 2: return qsTrId("taskboard_difficulty_expert_tooltip").arg(TextHelper.formatNumberWithThousandSeparators(root.expertLevelRequirement));
        case 3: return qsTrId("taskboard_difficulty_master_tooltip").arg(TextHelper.formatNumberWithThousandSeparators(root.masterLevelRequirement));
        }
        return "";
      }

      onCurrentValueChanged: {
        // Mouse wheel / keyboard navigation can still land on a greyed out difficulty: revert to the selected one.
        if (currentIndex !== root.bountyTaskDifficulty && !isItemEnabled(currentIndex)) {
          currentIndex = Qt.binding(function () { return root.bountyTaskDifficulty; });
          return;
        }
        selectBountyTaskDifficulty(currentIndex);
      }
    }

    Image {
      source: "/images/taskboard/icon-difficulty.png"
      opacity: eliteCheckbox.checked ? 1.0 : 0.4
      Layout.leftMargin: TibiaStyle.marginUnrelated
      Layout.alignment: Qt.AlignVCenter

      Tooltip {
        anchors.fill: parent
        text: qsTrId("taskboard_elite_tooltip")
        maxWidth: TibiaStyle.tooltipRestrictedWidth
      } //Tooltip
    }

    TibiaCheckBox {
      id: eliteCheckbox
      text: qsTrId("taskboard_elite_label")
      tooltipText: qsTrId("taskboard_elite_tooltip")
      Layout.alignment: Qt.AlignVCenter
      Layout.rightMargin: TibiaStyle.marginUnrelated
      shouldBeChecked: root.isElite
      onCheckedChanged: root.setEliteAction(checked)
    }

    TibiaButton {
      text: qsTrId("taskboard_preferred_list")
      Layout.preferredWidth: TibiaStyle.buttonWidthWider
      Layout.margins: TibiaStyle.marginUnrelated

      onClicked: {
        showPreferredList()
      }
    }

    Item {
      Layout.fillWidth: true
    }

    TibiaVerticalSeparator {
      Layout.margins: TibiaStyle.marginUnrelated
      Layout.preferredHeight: 20
    }

    TibiaButton {
      id: rerollButton
      text: qsTrId("taskboard_reroll_tasks")
      Layout.preferredWidth: TibiaStyle.buttonWidthWider
      Layout.margins: TibiaStyle.marginUnrelated

      enabled: canRerollTasks
      onClicked: rerollTasksAction()
      
      Tooltip {
        anchors.fill: parent
        enabled: !rerollButton.enabled
        text: qsTrId("taskboard_tooltip_no_rerolls_left")
      }
    }

    TibiaCurrencyView {
      id: upgradeCostControl
      Layout.preferredWidth: 40

      rightAligned: true
      iconId: "TaskReroll"
      balance: rerollTaskCost
    }

    Image {
      source: "/images/icon-inline-warning.png"
      visible: limitOfRerollTokensReached
      Tooltip {
        anchors.fill: parent
        text: qsTrId("no_claim_reroll_token_reason_tooltip_maximum_reached")
      }
    } // Image
  }
}
