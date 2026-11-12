#pragma once

#include "config/hotkeyoptions.h"
#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

#include <QPointer>
#include <QSet>
#include <set>

namespace tibia {
namespace gamewindow {

class TActionBarController;
class TPassiveAbilityListDataModel;
class TPassiveAbilityListDataModelSortFilterProxy;

/// The spells of spells.json for SpellList.qml (CIP: TSpellListDataModel, the
/// roles enumId, name, formulaRaw, iconUrl, iconUrlSmall, castPossible,
/// parameterHints, isCrossHairSpell, groupPrimaryIconUrl,
/// minimumCasterLevel), filtered by filterText / showOnlyKnown.
class TSpellListDataModel final : public cip::tibia::gamewindow::TSpellListDataModel
{
    Q_OBJECT
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
public:
    explicit TSpellListDataModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString filterText() const { return m_filterText; }
    void setFilterText(const QString& text);
    void setShowOnlyKnown(bool showOnlyKnown);
    int rowOf(int spellId) const;

public Q_SLOTS:
    /// CIP moc private slot — qtclient rebuilds rows (Magical Archive notifies).
    void onSpellDataModificationsApplied();

Q_SIGNALS:
    void filterTextChanged();

private:
    void rebuild();

    QString m_filterText;
    bool m_showOnlyKnown = false;
    std::vector<int> m_rows;
};

/// Shared action-bar bind/assign state for the CIP assign dialogs (not a QObject
/// parent — each dialog inherits `TDialogController<cip::…>` so qtclient_inherited
/// sees the CIP moc surface).
class TAssignActionDialogState
{
public:
    void bind(QObject* dialog, TActionBarController* bar, int buttonId, int multiActionIndex);
    config::TActionSetting current() const;
    void assign(const config::TActionSetting& setting) const;

    QPointer<TActionBarController> bar;
    int multiActionIndex = 0;
    uint actionBarId = 0;
    uint buttonId = 0;
    QString buttonName;
    bool showActionBarTarget = false;
};

/// AssignSpellActionDialog.qml (CIP: TAssignSpellActionDialogController,
/// v37). Cast type is kept on the action-bar setting
/// (crosshair / aim-at-target wave / none).
class TAssignSpellActionDialogController final
    : public TDialogController<cip::tibia::gamewindow::TAssignSpellActionDialogController>
{
    Q_OBJECT
public:
    TAssignSpellActionDialogController(TActionBarController* bar, int buttonId, int multiActionIndex, int preselectedSpellId,
                                       QObject* parent = nullptr);

    /// CIP moc property getters / showOnlyKnown WRITE.
    uint actionBarId() const override;
    uint buttonId() const override;
    QString buttonName() const override;
    bool showActionBarTarget() const override;
    uint spellListIndex() const override;
    QString spellParameters() const override;
    bool showOnlyKnownSpells() const override;
    void setShowOnlyKnownSpells(bool v) override;
    ::cip::tibia::qmlcomponents::enums::ESpellCastOptionalTargetType currentSpellCastType() const override;
    QVariant spellListModel() const override;

public Q_SLOTS:
    void onOkClicked(const QString& Parameters, ::cip::tibia::qmlcomponents::enums::ESpellCastOptionalTargetType CastType) override;
    void onApplyClicked(const QString& Parameters, ::cip::tibia::qmlcomponents::enums::ESpellCastOptionalTargetType CastType) override;
    void spellSelected(uint SpellEnumId) override;
    void onCloseClicked() override;

private:
    TAssignActionDialogState m_assign;
    TSpellListDataModel m_model;
    int m_spellId = 0;
};

/// AssignObjectActionDialog.qml (CIP: TAssignObjectActionDialogController).
class TAssignObjectActionDialogController final
    : public TDialogController<cip::tibia::gamewindow::TAssignObjectActionDialogController>
{
    Q_OBJECT
public:
    TAssignObjectActionDialogController(TActionBarController* bar, int buttonId, int multiActionIndex, QObject* parent = nullptr);

    /// Shows `objectId` with CIP's default use for it.
    void setObject(int objectId, int upgradeTier);

    /// CIP moc property getters.
    uint actionBarId() const override;
    uint buttonId() const override;
    QString buttonName() const override;
    bool showActionBarTarget() const override;
    uint objectID() const override;
    uint objectCount() const override;
    uint upgradeTier() const override;
    ::cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType::ELiquidType liquidType() const override;
    ::cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType::EHookDirection hookDirection() const override;
    uint useObjectType() const override;
    bool canBeEquiped() const override;
    bool canBeMultiUsed() const override;
    bool canHaveSmartMode() const override;
    bool smartModeEnabled() const override;
    bool clientSettingSmoothFiltering() const override;

public Q_SLOTS:
    void onOkClicked(uint UseObjectTypeEnumId, bool UseEquipSmartModeEnabled) override;
    void onApplyClicked(uint UseObjectTypeEnumId, bool UseEquipSmartModeEnabled) override;
    /// "Select Object": the next click on the map or an inventory slot picks it.
    void startTargetSelection() override;
    void onCloseClicked() override;
    /// CIP moc handleGameAction(shared_ptr<IGameAction>) — qtclient live path is
    /// startTargetSelection → input::startTargetSelection.
    void handleGameAction();

private:
    TAssignActionDialogState m_assign;
};

/// AssignTextActionDialog.qml (CIP: TAssignTextActionDialogController).
class TAssignTextActionDialogController final
    : public TDialogController<cip::tibia::gamewindow::TAssignTextActionDialogController>
{
    Q_OBJECT
public:
    TAssignTextActionDialogController(TActionBarController* bar, int buttonId, int multiActionIndex, QObject* parent = nullptr);

    /// CIP moc property getters.
    uint actionBarId() const override;
    uint buttonId() const override;
    QString buttonName() const override;
    bool showActionBarTarget() const override;
    QString text() const override;
    bool sendAutomatically() const override;

public Q_SLOTS:
    void onOkClicked(const QString& Text, bool SendAutomatically) override;
    void onApplyClicked(const QString& Text, bool SendAutomatically) override;
    void onCloseClicked() override;

private:
    TAssignActionDialogState m_assign;
};

/// AssignPassiveAbilityActionDialog.qml (CIP:
/// TAssignPassiveAbilityActionDialogController, v37).
class TAssignPassiveAbilityActionDialogController final
    : public TDialogController<cip::tibia::gamewindow::TAssignPassiveAbilityActionDialogController>
{
    Q_OBJECT
public:
    TAssignPassiveAbilityActionDialogController(TActionBarController* bar, int buttonId, QObject* parent = nullptr);

    /// CIP moc property getters.
    uint actionBarId() const override;
    uint buttonId() const override;
    QString buttonName() const override;
    bool showActionBarTarget() const override;
    uint passiveAbilityListIndex() const override;
    QString passiveAbilityName() const override;
    QString passiveAbilityIconUrl() const override;
    QVariant passiveAbilityListModel() const override;

public Q_SLOTS:
    void onOkClicked() override;
    void onApplyClicked() override;
    void passiveAbilitySelected(uint PassiveAbilityEnumId) override;
    void onCloseClicked() override;

private:
    void refreshSelectedPassive();

    TAssignActionDialogState m_assign;
    TPassiveAbilityListDataModel* m_model = nullptr;
    TPassiveAbilityListDataModelSortFilterProxy* m_proxy = nullptr;
    int m_passiveAbility = 0;
};

/// RecordKeyboardShortcutDialog.qml (CIP: TRecordKeyboardShortcutDialogController
/// with TRecordKeyboardShortcutHandler): records the next key while shown;
/// OK binds it to TriggerActionButton_<bar>.<button> in the current chat mode,
/// or only emits keySequenceSelected when constructed for the options page.
class TRecordKeyboardShortcutDialogController final
    : public TDialogController<cip::tibia::gamewindow::TRecordKeyboardShortcutDialogController>
{
    Q_OBJECT
public:
    TRecordKeyboardShortcutDialogController(int actionBarId, int buttonId, bool chatModeOn, QObject* parent = nullptr);
    /// Options-page capture (CIP +0x3a8): OK emits keySequenceSelected, does not write hotkeys.
    /// `actionName` is the static action id for occupied-key checks (same action is not "occupied").
    TRecordKeyboardShortcutDialogController(const QString& description, const QString& actionName, bool chatModeOn,
                                           const QKeySequence& current, QObject* parent = nullptr);
    ~TRecordKeyboardShortcutDialogController() override;

    /// CIP moc property getters.
    QString keySequence() const override;
    bool keySequenceOccupied() const override;
    bool keySequenceHardcoded() const override;
    QString actionDescription() const override;
    bool chatModeOn() const override;
    bool onlySingleKey() const override;

public Q_SLOTS:
    /// CIP moc showDialog — qtclient constructors seed the same fields, then show.
    void showDialog(bool ChatModeOn, bool OnlySingleKey, const QKeySequence& CurrentKeySequence, const QString& ActionDescription,
                    const std::set<QKeySequence>& OccupiedKeys);
    void onKeySequenceEntered(const QKeySequence& NewKeySequence) override;
    void onOkButtonClicked() override;
    void onClearButtonClicked() override;
    void onCancelButtonClicked() override;
    void requestClose() override;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    int m_actionBarId = 0;
    int m_buttonId = 0;
    bool m_bindOnOk = true;
    QString m_actionName;
    QKeySequence m_keys;
    QSet<QKeySequence> m_occupiedKeys;
};

}
}
