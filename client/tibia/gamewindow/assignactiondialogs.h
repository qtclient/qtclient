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

/// The button a bar's assign dialog edits (CIP TAssignActionDialogControllerBase
/// +0x3c button, +0x40 multi-action slot, +0x44 showActionBarTarget).
class TAssignActionDialogState
{
public:
    void bind(const TActionBarController* bar, uint buttonId, int multiActionIndex);

    int multiActionIndex = -1;
    uint actionBarId = 0;
    uint buttonId = 0;
    QString buttonName;
    bool showActionBarTarget = true;
};

using ECastType = ::cip::tibia::qmlcomponents::enums::ESpellCastOptionalTargetType;

/// AssignSpellActionDialog.qml (CIP: TAssignSpellActionDialogController, one
/// per action bar). OK and Apply ask the bar to assign the spell.
class TAssignSpellActionDialogController final
    : public TDialogController<cip::tibia::gamewindow::TAssignSpellActionDialogController>
{
    Q_OBJECT
public:
    explicit TAssignSpellActionDialogController(TActionBarController* bar, QObject* parent = nullptr);

    /// CIP v26: the list's first spell, no parameters.
    void showDialog(uint ButtonId, int MultiActionIndex);
    /// CIP v33: a spell with its parameters and cast type.
    void showDialog(uint ButtonId, int MultiActionIndex, int SpellId, const QString& Parameters, ECastType CastType);

    uint actionBarId() const override;
    uint buttonId() const override;
    QString buttonName() const override;
    bool showActionBarTarget() const override;
    uint spellListIndex() const override;
    QString spellParameters() const override;
    bool showOnlyKnownSpells() const override;
    void setShowOnlyKnownSpells(bool v) override;
    ECastType currentSpellCastType() const override;
    QVariant spellListModel() const override;

public Q_SLOTS:
    void onOkClicked(const QString& Parameters, ECastType CastType) override;
    void onApplyClicked(const QString& Parameters, ECastType CastType) override;
    void spellSelected(uint SpellEnumId) override;
    void onCloseClicked() override;

Q_SIGNALS:
    void requestAssignSpell(uint ButtonId, int MultiActionIndex, int SpellId, const QString& Parameters, ECastType CastType);

private:
    const TActionBarController* m_bar;
    TAssignActionDialogState m_assign;
    TSpellListDataModelSortFilterProxy m_proxy;
    int m_spellId = 0;
};

/// AssignObjectActionDialog.qml (CIP: TAssignObjectActionDialogController,
/// one per action bar). OK and Apply ask the bar to assign the object.
class TAssignObjectActionDialogController final
    : public TDialogController<cip::tibia::gamewindow::TAssignObjectActionDialogController>
{
    Q_OBJECT
public:
    explicit TAssignObjectActionDialogController(TActionBarController* bar, QObject* parent = nullptr);

    /// CIP v26: no object yet, so the next click picks one.
    void showDialog(uint ButtonId, int MultiActionIndex);
    /// CIP v33: shows an object with its use; with `assignWithoutDialog` it is
    /// assigned at once instead.
    void showDialog(uint ButtonId, int MultiActionIndex, int ObjectId, int UpgradeTier, int LiquidType, config::EUseType UseType,
                    bool SmartModeEnabled, bool assignWithoutDialog);
    /// CIP v34 (qtclient's appearance data): an object picked or dropped, with
    /// its default use.
    void showDialogForObject(uint ButtonId, int MultiActionIndex, int ObjectId, int UpgradeTier, bool assignWithoutDialog);
    /// The smooth filtering client setting the preview uses.
    void setClientSettingSmoothFiltering(bool enabled);

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
    /// "Select Object": the dialog hides and the next click on the map or an
    /// inventory slot picks the object.
    void startTargetSelection() override;
    void onCloseClicked() override;

Q_SIGNALS:
    void requestAssignObject(uint ButtonId, int MultiActionIndex, int ObjectId, int UpgradeTier, int LiquidType,
                             tibia::config::EUseType UseType, bool UseEquipSmartMode);

private:
    void setObject(int objectId, int upgradeTier);
    void setUseObjectType(uint useObjectType);
    void setSmartModeEnabled(bool enabled);
    void emitRequestAssignObject();

    TAssignActionDialogState m_assign;
    const TActionBarController* m_bar;
    /// A target selection runs for this dialog.
    bool m_selectingTarget = false;
};

/// AssignTextActionDialog.qml (CIP: TAssignTextActionDialogController, one
/// per action bar). OK and Apply ask the bar to assign the text.
class TAssignTextActionDialogController final
    : public TDialogController<cip::tibia::gamewindow::TAssignTextActionDialogController>
{
    Q_OBJECT
public:
    explicit TAssignTextActionDialogController(TActionBarController* bar, QObject* parent = nullptr);

    /// CIP v26: an empty text, sent automatically.
    void showDialog(uint ButtonId, int MultiActionIndex);
    /// CIP v33.
    void showDialog(uint ButtonId, int MultiActionIndex, const QString& Text, bool SendAutomatically);

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

Q_SIGNALS:
    void requestAssignText(uint ButtonId, int MultiActionIndex, const QString& Text, bool SendAutomatically);

private:
    const TActionBarController* m_bar;
    TAssignActionDialogState m_assign;
};

/// AssignPassiveAbilityActionDialog.qml (CIP:
/// TAssignPassiveAbilityActionDialogController, one per action bar).
class TAssignPassiveAbilityActionDialogController final
    : public TDialogController<cip::tibia::gamewindow::TAssignPassiveAbilityActionDialogController>
{
    Q_OBJECT
public:
    explicit TAssignPassiveAbilityActionDialogController(TActionBarController* bar, QObject* parent = nullptr);

    /// CIP v26: the list's first ability.
    void showDialog(uint ButtonId);
    /// CIP v33.
    void showDialog(uint ButtonId, int PassiveAbility);

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

Q_SIGNALS:
    void requestAssignPassiveAbility(uint ButtonId, int PassiveAbility);

private:
    void refreshSelectedPassive();
    /// The abilities the storage knows now.
    void reloadAbilities();

    const TActionBarController* m_bar;
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
    /// A bar's recorder (CIP: TActionBarController +0x3d8): showDialog opens it,
    /// keySequenceSelected hands the key to the bar.
    explicit TRecordKeyboardShortcutDialogController(QObject* parent = nullptr);
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
public:
    /// The keys bound to other actions in the chat mode.
    static std::set<QKeySequence> occupiedKeys(const QString& ownAction, bool chatModeOn);

private:
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
