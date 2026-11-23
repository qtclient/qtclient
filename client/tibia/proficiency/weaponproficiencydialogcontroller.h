#pragma once

#include "input/gameactiondispatch.h"
#include "client/gamecore.h"
#include "gamewindow/dialogmanager.h"
#include "proficiency/proficiencystorage.h"
#include "qtconnectionmanager.h"

#include <cip_gen.h>

#include <QCollator>
#include <QPointer>

#include <functional>
#include <memory>
#include <set>
#include <vector>

namespace tibia::game {
class IGameSession;
}

struct ItemInspectionData;

namespace tibia {
namespace gamewindow {
class TMessageDialogController;
}
}

namespace tibia {
namespace proficiency {

/// A weapon of the dialog's grid.
struct TWeaponEntry
{
    uint16_t typeId = 0;
    QString name;
    /// The appearances' WEAPON_TYPE.
    uint32_t weaponType = 0;
    uint32_t minimumLevel = 0;
    std::vector<uint32_t> professions;
    bool hasClothSlot = false;
    int clothSlot = 0;
};

/// The weapons with a proficiency (CIP: the TAppearanceTypeListModel the
/// dialog controller makes in its ctor).
class TWeaponObjectsModel final : public cip::tibia::gamewindow::TAppearanceTypeListModel
{
    Q_OBJECT
public:
    enum Role
    {
        TypeID = Qt::UserRole + 1,
        DisplayTypeID,
        CumulativeCount,
        TypeCount,
        CountString,
        TypeName,
        LiquideType,
        HookDirection,
        LootvalueImageSourceUnderItem,
        LootvalueImageSourceOverItem,
        UpgradeTier,
        DecoItemObjectID,
        ProficiencyXP,
        ProficiencyLevel,
        HasProficiencyMastery,
    };

    explicit TWeaponObjectsModel(TProficiencyStorage* storage, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void rebuild();
    bool isEmpty() const { return m_entries.empty(); }
    const TWeaponEntry& entry(int row) const { return m_entries[row]; }
    int size() const { return static_cast<int>(m_entries.size()); }
    bool contains(uint16_t typeId) const
    {
        return std::ranges::any_of(m_entries, [typeId](const TWeaponEntry& entry) { return entry.typeId == typeId; });
    }
    uint32_t xp(int row) const { return m_storage->xp(m_entries[row].typeId); }

    void onObjectProficiencyXPChanged() override;

private:
    /// CIP : the loot value frames follow colorizeLootStyle.
    void onLootValueImagesChanged();
    QString lootValueImageSource(uint16_t typeId, int style) const;

    TProficiencyStorage* m_storage;
    std::vector<TWeaponEntry> m_entries;
    int m_colorizeLootStyle = 0;
    QMetaObject::Connection m_pricesConnection;
};

/// The grid's filters (CIP: TAppearanceTypeListModelSortFilterProxy with
/// the weapon-type set of setWeaponTypeFilter).
class TWeaponObjectsProxy final : public cip::tibia::gamewindow::TAppearanceTypeListModelSortFilterProxy
{
    Q_OBJECT
public:
    explicit TWeaponObjectsProxy(TWeaponObjectsModel* model, QObject* parent = nullptr);

    void clearFilters();
    void setNameFilter(const QString& name);
    void setWeaponTypes(const std::set<int>& types);
    void setFilter(bool byLevel, bool byVocation, bool oneHand, bool twoHand);
    /// The proxy row of `typeId`, -1 when filtered out.
    int rowOf(uint16_t typeId) const;
    uint16_t typeIdAt(int row) const;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    TWeaponObjectsModel* m_model;
    QString m_name;
    std::set<int> m_weaponTypes;
    QCollator m_collator;
};

/// One entry of the weapon-type combo box (CIP: TQmlWeaponTypes).
class TQmlWeaponTypes final : public cip::tibia::gamewindow::TQmlWeaponTypes
{
    Q_OBJECT
public:
    TQmlWeaponTypes(const QString& text, std::set<int> types, QObject* parent);
    const std::set<int>& types() const { return m_types; }
    /// The first weapon type, 0 without any.
    int anyWeaponIdentifier() const override;

private:
    std::set<int> m_types;
};

/// WeaponProficiencyDialog.qml (CIP: TWeaponProficiencyDialogController,
/// ctor , game action OPEN_WEAPON_PROFICIENCY_DIALOG).
class TWeaponProficiencyDialogController final
    : public gamewindow::TDialogController<cip::tibia::gamewindow::TWeaponProficiencyDialogController>
{
    Q_OBJECT
public:
    /// The levels of a weapon's proficiency table.
    int maxProficiencyLevels() const override;
    explicit TWeaponProficiencyDialogController(TProficiencyStorage* storage, QObject* parent = nullptr);
    ~TWeaponProficiencyDialogController() override;

    /// CIP .
    void showDialog(uint16_t typeId);
    /// CIP connector: the dialog closes; the weapons list, the proficiency
    /// storage, the session's inspection replies, player data and forge dust
    /// follow the session (null: none).
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

    QString currentTypeName() const override;
    bool currentIsUseRestricted() const override;
    QString currentDecorationSource() const override;
    QString currentDecorationLevelSource() const override;
    uint currentProficiencyXP() const override;
    uint nextLevelProficiencyXP() const override;
    uint masteryProficiencyXP() const override;
    bool canEditPerks() const override;
    bool currentProficiencyChanged() const override;
    bool hasPickedPerks() const override;
    QString modifiyDustCost() const override;
    bool hasEnoughDustToModify() const override;

    void requestClose() override;
    void onOkClicked() override;
    void onApplyClicked() override;
    void weaponObjectSelected(uint RawTypeId) override;
    void setWeaponTypeFilter(int RawWeaponType) override;
    void setNameFilter(const QString& Name) override;
    void setFilter(bool FilterByLevel, bool FilterByVocation, bool OnlyOneHandWeapons, bool OnlyTwoHandWeapons) override;
    void pickPerk(uint LevelIndex, uint PerkIndex) override;
    void resetPerks() override;
    void selectPerk(int LevelIndex, int PerkIndex) override;
    void onModifyShapeClicked() override;
    void closeDialog() override;

private:
    /// Makes `typeId` the current weapon: false when the list does not have
    /// it or it already is the current one.
    bool select(uint16_t typeId);
    /// Asks the server for the weapon's proficiency inspection.
    void sendInspectObject(uint16_t typeId) const;
    /// CIP .
    void rebuildModel();
    /// CIP .
    void updateSelectedPerk();
    /// CIP .
    bool hasEnoughDust() const;
    /// The local perk choices differ from the server's record of the current weapon.
    bool localDiffersFromServer() const;
    /// Two pick lists are equal: same length, same levels, same picked flags and,
    /// where both are picked, the same perk.
    static bool picksEqual(const std::vector<TPerkPick>& a, const std::vector<TPerkPick>& b);
    /// The server's record of a weapon changed; the current weapon takes it over.
    void onProficiencyDataChanged(uint16_t typeId);
    /// CIP / : asks to save unsaved perks, then runs `next`.
    void confirmUnsavedChanges(std::function<void()> next);
    void sendItemInfo(uint16_t typeId) const;
    /// CIP: the inspection reply for the current weapon.
    void onInspectObjectDataReceived(const ItemInspectionData& data);
    void onPlayerDataChanged();
    void onExaltedCurrencyBalanceChanged();

    TProficiencyStorage* m_storage;
    TWeaponObjectsModel m_model;
    TWeaponObjectsProxy m_proxy;
    TProficiencyData m_local;
    bool m_pendingReset = false;
    gamewindow::TMessageDialogController* m_confirm;
    std::function<void()> m_onAccept;
    std::function<void()> m_onDiscard;
    std::shared_ptr<game::IGameSession> m_gameSession;
    shared::TQtConnectionManager m_gameSessionConnections;

public Q_SLOTS:
    /// The router's game actions (CIP: handleGameAction; connected to
    /// publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
    {
        m_gameActions.handle(pGameAction);
    }

private:
    ::tibia::input::TGameActionDispatch m_gameActions;
};

}
}
