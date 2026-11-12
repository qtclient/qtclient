import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import qmlcomponents


TibiaDialog {
  id: root
  caption: controller.showActionBarTarget
    ? qsTrId("assign_spell_dialog_caption").arg(actionBarName)
    : qsTrId("assign_spell_dialog_caption_short")

  width: 375

  required property var controller
  property var currentModelData: null
  property bool spellSelected: currentModelData != null
  property string actionBarName: controller.buttonName
  property bool spellNeedsParameters: currentModelData?.parameterHints.length > 0 ?? false
  property bool spellIsCrossHairSpell: currentModelData?.isCrossHairSpell ?? false
  property bool spellIsAimAtTargetConfigurable: currentModelData?.isAimAtTargetConfigurable ?? false

  readonly property int effectiveSpellTargetTypeEnumID: {
    if (spellIsCrossHairSpell) {
      return targetCastTypeGroup.spellTargetTypeEnumID;
    } else if (spellIsAimAtTargetConfigurable) {
      return waveCastTypeGroup.spellTargetTypeEnumID;
    }
    return TibiaEnums.ESpellCastOptionalTargetType.None;
  } //effectiveSpellTargetTypeEnumID

  onReturnPressedFunction: okClicked
  onCancelPressedFunction: closeClicked
  initialFocusItem: searchField

  function okClicked() {
    controller.onOkClicked(
      parameterTextField.text,
      effectiveSpellTargetTypeEnumID);
  } //function okClicked

  function assignSpell() {
    controller.onApplyClicked(
      parameterTextField.text,
      effectiveSpellTargetTypeEnumID);
  } //function assignSpell

  function closeClicked() {
    controller.onCloseClicked();
  } //function closeClicked

  function setParameters() {
    parameterTextField.text = controller.spellParameters
  } //function setParameters()

  function setSpellCastType() {
    targetCastTypeGroup.spellTargetTypeEnumID = controller.currentSpellCastType;
    waveCastTypeGroup.spellTargetTypeEnumID = targetCastTypeGroup.spellTargetTypeEnumID;

    if (spellIsCrossHairSpell
      && targetCastTypeGroup.spellTargetTypeEnumID == TibiaEnums.ESpellCastOptionalTargetType.None) {
      targetCastTypeGroup.spellTargetTypeEnumID = TibiaEnums.ESpellCastOptionalTargetType.Crosshair; // crosshair is default
    }
  }

  ColumnLayout {
    anchors { left: parent.left; right: parent.right }
    spacing: TibiaStyle.marginUnrelated

    RowLayout {
      Layout.fillWidth: true
      spacing: TibiaStyle.marginRelated

      Image {
        Layout.preferredHeight: TibiaStyle.spellListIconSize
        source: currentModelData?.iconUrl ?? ""
        onSourceChanged: {
          root.setParameters(); //neede to react on the change of the current spell
          root.setSpellCastType();
        }
      } //Image

      ColumnLayout {
        spacing: 0
        Layout.fillWidth: true

        TibiaText {
          Layout.fillWidth: true
          text: currentModelData?.name ?? ""
          visible: root.spellSelected
        } // TibiaText

        TibiaText {
          Layout.fillWidth: true
          visible: !root.spellSelected
          text: qsTrId("assign_spell_dialog_no_spell_selected")
        } //TibiaText

        TibiaText {
          Layout.fillWidth: true
          text: (currentModelData?.formulaRaw ?? "")
            + (parameterTextField.text != "" ? " " + parameterTextField.text : "")
        } // TibiaText
      } //ColumnLayout
    } //RowLayout

    TibiaHorizontalSeparator {
      Layout.fillWidth: true
    } //TibiaHorizontalSeparator

    ColumnLayout {
      Layout.fillWidth: true
      spacing: TibiaStyle.marginRelated

      TibiaFrame1PixelDown {
        Layout.fillWidth: true
        Layout.preferredHeight: 380

        ColumnLayout {
          anchors.fill: parent
          anchors.margins: parent.borderWidth

          TibiaTextSearchField {
            id: searchField
            Layout.fillWidth: true
            onSearchTextChanged: {
              if (spellList.model != null) {
                spellList.model.filterText = searchText;
              }
            } //onSearchTextChanged
          } //TibiaTextSearchField

          TibiaScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: spellList.width

            SpellList {
              id: spellList

              KeyNavigation.tab: parameterTextField

              model: controller.spellListModel
              externalSelectedIndex: controller.spellListIndex
              showOnlyKnownSpells: controller.showOnlyKnownSpells
              showSpellgroup: true

              onSpellSelected: (spellEnumId) => {
                controller.spellSelected(spellEnumId);
                var _helperModel = AbstractItemModelHelper.wrapInHelperProxyModel(model);
                var newModelData = null;
                for (var i = 0; i < _helperModel.rowCount(); i++) {
                  var tempModelData = _helperModel.sourceItemDataByRowIndex(i);
                  if (spellEnumId == tempModelData.enumId) {
                    newModelData = tempModelData;
                    break;
                  }
                }
                currentModelData = newModelData;
              } //onSpellSelected
            } //SpellList
          } //TibiaScrollView
        } //ColumnLayout
      } // TibiaFrame1PixelDown

      TibiaMenuOptionCheckBox {
        id: showOnlyKnownSpells
        text: qsTrId("assign_spell_dialog_label_showonlyknownspells")
        Layout.fillWidth: true
        checked: controller.showOnlyKnownSpells
        Binding {
          target: controller
          property: "showOnlyKnownSpells"
          value: showOnlyKnownSpells.checked
        } //Binding
      } //TibiaMenuOptionCheckBox

      RowLayout {
        Layout.fillWidth: true
        spacing: TibiaStyle.marginRelated
        enabled: spellNeedsParameters

        TibiaText {
          text: qsTrId("assign_spell_dialog_label_parameters")
        } //TibiaText

        TibiaTextField {
          id: parameterTextField
          Layout.fillWidth: true
          KeyNavigation.tab: spellList
          text: controller.spellParameters
          maximumLength: TibiaStyle.chatInputMaxLength - (currentModelData ? currentModelData.formulaRaw.length + 1 : 0)
          placeholderText: currentModelData?.parameterHints ?? ""
        } //TibiaTextField
      } //RowLayout

      ColumnLayout {
        Layout.fillWidth: true
        spacing: TibiaStyle.marginRelated

        TibiaText {
          id: targetOptionsLabel
          Layout.fillWidth: true
          text: qsTrId("assign_spell_dialog_label_target_options")
          enabled: positionTargetingLayout.visible || waveTargetingLayout.visible
        } //TibiaText

        TibiaText {
          Layout.minimumHeight: Math.max(positionTargetingLayout.height, waveTargetingLayout.height)
          text: qsTrId("none")
          enabled: false
          visible: !targetOptionsLabel.enabled
        } //TibiaText

        ColumnLayout {
          id: positionTargetingLayout
          Layout.fillWidth: true
          spacing: TibiaStyle.marginRelated
          visible: root.spellIsCrossHairSpell

          ButtonGroup {
            id: targetCastTypeGroup
            property int spellTargetTypeEnumID: TibiaEnums.ESpellCastOptionalTargetType.None

            checkedButton: {
              if (controller.currentSpellCastType == TibiaEnums.ESpellCastOptionalTargetType.Crosshair) {
                return castTypeCrossHair;
              } else if (controller.currentSpellCastType == TibiaEnums.ESpellCastOptionalTargetType.AtMousePosition) {
                return castTypeMouseCursor;
              } else if (controller.currentSpellCastType == TibiaEnums.ESpellCastOptionalTargetType.AtTargetOrSelf) {
                return castTypeAtTargetOrSelf;
              }

              return castTypeCrossHair;
            }

            onCheckedButtonChanged: {
              if (checkedButton == castTypeCrossHair) {
                targetCastTypeGroup.spellTargetTypeEnumID = TibiaEnums.ESpellCastOptionalTargetType.Crosshair;
              } else if (checkedButton == castTypeMouseCursor) {
                targetCastTypeGroup.spellTargetTypeEnumID = TibiaEnums.ESpellCastOptionalTargetType.AtMousePosition;
              } else if (checkedButton == castTypeAtTargetOrSelf) {
                targetCastTypeGroup.spellTargetTypeEnumID = TibiaEnums.ESpellCastOptionalTargetType.AtTargetOrSelf;
              } else {
                targetCastTypeGroup.spellTargetTypeEnumID = TibiaEnums.ESpellCastOptionalTargetType.None;
              }
            } //onCurrentChanged
          } //ButtonGroup

          TibiaRadioButton {
            id: castTypeCrossHair
            text: qsTrId("assign_spell_dialog_option_crosshair")
            ButtonGroup.group: targetCastTypeGroup
          } //TibiaRadioButton

          TibiaRadioButton {
            id: castTypeMouseCursor
            text: qsTrId("assign_spell_dialog_option_mousecursor")
            ButtonGroup.group: targetCastTypeGroup
          } //TibiaRadioButton

          TibiaRadioButton {
            id: castTypeAtTargetOrSelf
            text: qsTrId("assign_spell_dialog_option_target")
            ButtonGroup.group: targetCastTypeGroup
          } //TibiaRadioButton
        } // ColumnLayout

        ColumnLayout {
          id: waveTargetingLayout
          Layout.fillWidth: true
          spacing: TibiaStyle.marginRelated
          visible: root.spellIsAimAtTargetConfigurable

          ButtonGroup {
            id: waveCastTypeGroup
            property int spellTargetTypeEnumID: TibiaEnums.ESpellCastOptionalTargetType.None

            checkedButton: {
              if (controller.currentSpellCastType == TibiaEnums.ESpellCastOptionalTargetType.None) {
                return castTypeTowardsFacing;
              } else if (controller.currentSpellCastType == TibiaEnums.ESpellCastOptionalTargetType.AtMousePosition) {
                return castTypeTowardsMouseCursor;
              } else if (controller.currentSpellCastType == TibiaEnums.ESpellCastOptionalTargetType.AtTargetOrSelf) {
                return castTypeTowardsTarge;
              }

              return castTypeTowardsFacing;
            }

            onCheckedButtonChanged: {
              if (checkedButton == castTypeTowardsFacing) {
                waveCastTypeGroup.spellTargetTypeEnumID = TibiaEnums.ESpellCastOptionalTargetType.None;
              } else if (checkedButton == castTypeTowardsMouseCursor) {
                waveCastTypeGroup.spellTargetTypeEnumID = TibiaEnums.ESpellCastOptionalTargetType.AtMousePosition;
              } else if (checkedButton == castTypeTowardsTarge) {
                waveCastTypeGroup.spellTargetTypeEnumID = TibiaEnums.ESpellCastOptionalTargetType.AtTargetOrSelf;
              } else {
                waveCastTypeGroup.spellTargetTypeEnumID = TibiaEnums.ESpellCastOptionalTargetType.None;
              }
            } //onCurrentChanged
          } //ButtonGroup

          TibiaRadioButton {
            id: castTypeTowardsFacing
            text: qsTrId("assign_spell_dialog_option_towards_facing")
            ButtonGroup.group: waveCastTypeGroup
          } //TibiaRadioButton

          TibiaRadioButton {
            id: castTypeTowardsMouseCursor
            text: qsTrId("assign_spell_dialog_option_towards_mousecursor")
            ButtonGroup.group: waveCastTypeGroup
          } //TibiaRadioButton

          TibiaRadioButton {
            id: castTypeTowardsTarge
            text: qsTrId("assign_spell_dialog_option_towards_target")
            ButtonGroup.group: waveCastTypeGroup
          } //TibiaRadioButton
        } //ColumnLayout
      } //ColumnLayout
    } //ColumnLayout

    TibiaHorizontalSeparator {
      Layout.fillWidth: true
    } //TibiaHorizontalSeparator

    RowLayout {
      Layout.alignment: Qt.AlignRight
      spacing: TibiaStyle.marginRelated

      Item { Layout.fillWidth: true }

      TibiaButton {
        text: qsTrId("ok")
        onClicked: root.okClicked();
        enabled: root.spellSelected
      } // TibiaButton

      TibiaButton {
        text: qsTrId("apply")
        onClicked: root.assignSpell();
        enabled: root.spellSelected
      } // TibiaButton

      TibiaButton {
        text: qsTrId("cancel")
        onClicked: root.closeClicked();
      } // TibiaButton
    } // RowLayout
  } // ColumnLayout
} // TibiaDialog
