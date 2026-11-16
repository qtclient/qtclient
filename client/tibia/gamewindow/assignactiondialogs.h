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

/// Every spell of the spell table in id order, with the server's
/// modifications applied (CIP: TSpellListDataModel). The rows' roles feed
/// SpellList.qml, the Magical Archive and the vocation selection popup.
class TSpellListDataModel final : public cip::tibia::gamewindow::TSpellListDataModel
{
    Q_OBJECT
public:
    enum ERole
    {
        EnumIdRole = 0x101,
        NameRole,
        FormulaRawRole,
        ParameterHintsRole,
        GroupsRole,
        GroupPrimaryRole,
        GroupPrimaryIconUrlRole,
        GroupSecondaryRole,
        IconUrlRole,
        IconUrlSmallRole,
        IconIndexRole,
        TypeRole,
        DamagetypeRole,
        MinimumCasterLevelRole,
        MinimumMagicLevelRole,
        IsRuneRole,
        RuneAmountRole,
        IsPremiumRole,
        PremiumOnlyRole,
        AllowedVocationsRole,
        ManaCostRole,
        SoulPointsCostRole,
        CooldownsRole,
        CooldownRole,
        CooldownPrimaryGroupRole,
        CooldownSecondaryGroupRole,
        GroupCooldownsRole,
        CastPossibleRole,
        DescriptionRole,
        ScalingRole,
        SpellUsageInfoRole,
        RuneUsageInfoRole,
        RelevantUsageInfoRole,
        SourceRole,
        IsAimAtTargetConfigurableRole,
        IsCrossHairSpellRole,
    };

    explicit TSpellListDataModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

public Q_SLOTS:
    /// The modified roles (mana cost, cooldown) of every row changed.
    void onSpellDataModificationsApplied();

private:
    /// The table's spells in id order.
    std::vector<int> m_spellIds;
};

/// The spell list as a player browses it (CIP:
/// TSpellListDataModelSortFilterProxy): filtered by the player's vocation,
/// level and known spells, by vocation, group, premium and kind checkboxes
/// and by the search words, sorted by name. It owns its TSpellListDataModel.
class TSpellListDataModelSortFilterProxy final : public cip::tibia::gamewindow::TSpellListDataModelSortFilterProxy
{
    Q_OBJECT
public:
    explicit TSpellListDataModelSortFilterProxy(QObject* parent = nullptr);

    /// Checks every box except "only current level", clears the search words.
    void resetFilter() override;
    /// The source model's roles of a row as a map.
    QVariant getModelDataForRow(int RowIndex) override;
    /// The spell of a row, 0 past the end.
    int spellIdAt(int row) const;
    /// The row of a spell, -1 when filtered out.
    int rowOfSpell(int spellId) const;

    void setShowOnlyCurrentVocation(bool v) override;
    void setShowOnlyCurrentLevel(bool v) override;
    void setShowUnkownSpells(bool v) override;
    void setShowDruidSpells(bool v) override;
    void setShowKnightSpells(bool v) override;
    void setShowPaladinSpells(bool v) override;
    void setShowSorcererSpells(bool v) override;
    void setShowMonkSpells(bool v) override;
    void setShowAttackSpellGroup(bool v) override;
    void setShowHealingSpellGroup(bool v) override;
    void setShowSupportSpellGroup(bool v) override;
    void setShowRuneSpells(bool v) override;
    void setShowInstantSpells(bool v) override;
    void setShowPremiumOnlySpells(bool v) override;
    void setShowFreeSpells(bool v) override;
    void setFilterText(const QString& v) override;

    QVariant data(const QModelIndex& index, int role) const override;

public Q_SLOTS:
    QVariant getDataWithRoleName(int RowIndex, const QByteArray& RoleName) override;
    /// Re-reads the player's known spells, vocation and level; refilters on a change.
    void onPlayerDataChanged() override;
    /// Follows the new session's player data.
    void onGameSessionChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    void setFlag(bool& flag, bool value);

    TSpellListDataModel* m_model;
    /// The player's base vocation (CIP id) and level, 0 without a session.
    uint8_t m_playerVocation = 0;
    uint16_t m_playerLevel = 0;
    std::set<uint16_t> m_knownSpells;
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
    TSpellListDataModelSortFilterProxy m_proxy;
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
    /// An action bar button's key: OK writes it to the hotkey options.
    TRecordKeyboardShortcutDialogController(int actionBarId, int buttonId, bool chatModeOn, QObject* parent = nullptr);
    /// The options page's capture: OK only emits keySequenceSelected.
    /// `actionName` is the action whose own keys are not "occupied".
    TRecordKeyboardShortcutDialogController(const QString& description, const QString& actionName, bool chatModeOn,
                                           const QKeySequence& current, QObject* parent = nullptr);
    ~TRecordKeyboardShortcutDialogController() override;

    QString keySequence() const override;
    bool keySequenceOccupied() const override;
    bool keySequenceHardcoded() const override;
    QString actionDescription() const override;
    bool chatModeOn() const override;
    bool onlySingleKey() const override;
    /// The key the dialog was opened with.
    QKeySequence selectedKeySequence() const { return m_selectedKeys; }

public Q_SLOTS:
    void showDialog(bool ChatModeOn, bool OnlySingleKey, const QKeySequence& CurrentKeySequence, const QString& ActionDescription,
                    const std::set<QKeySequence>& OccupiedKeys);
    void onKeySequenceEntered(const QKeySequence& NewKeySequence) override;
    void onOkButtonClicked() override;
    void onClearButtonClicked() override;
    void onCancelButtonClicked() override;
    void requestClose() override;

protected:
    /// Stands in for CIP's key recorder: key presses while the dialog shows.
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void initKeyLists();
    std::set<QKeySequence> occupiedKeys(const QString& ownAction, bool chatModeOn) const;
    void startRecording();
    void stopRecording();

    /// The walk keys, which cannot be bound.
    QList<QKeySequence> m_hardcodedKeys;
    /// Modifiers alone, which do not make a key.
    QList<QKeySequence> m_modifierOnlyKeys;
    bool m_isChatModeOn = false;
    bool m_isOnlySingleKey = false;
    QKeySequence m_keys;
    QKeySequence m_selectedKeys;
    QString m_description;
    std::set<QKeySequence> m_occupiedKeys;
    bool m_recording = false;
};

}
}
