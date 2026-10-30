#pragma once

#include "gamewindow/messagedialogcontroller.h"

#include <cip_gen.h>

#include <QList>
#include <QSortFilterProxyModel>
#include <QString>
#include <QTimer>

#include <array>
#include <cstdint>
#include <set>
#include <vector>

namespace tibia::prey {

/// A creature of a prey slot (CIP: name and outfit of TPreyData's monster).
struct TPreyMonster
{
    QString name;
    uint outfitId = 0;
    int head = 0;
    int torso = 0;
    int legs = 0;
    int detail = 0;
    bool firstAddOn = false;
    bool secondAddOn = false;
};

/// The protocol's prey bonus (type 4: none).
struct TPreyBonus
{
    int type = 4;
    int value = 0;
    int grade = 0;
};

/// One prey slot (CIP: TPreyData, ctor 0x100ee2250, 0xd8 bytes). States:
/// 0 locked, 1 inactive, 2 active, 3 selection, 4 selection with a bonus,
/// 5 bestiary selection, 6 bestiary selection with a bonus (wildcard).
class TPreyData final : public cip::tibia::prey::TPreyData
{
    Q_OBJECT
public:
    explicit TPreyData(uint8_t preyId, QObject* parent = nullptr);

    uint8_t preyId() const { return m_preyId; }
    int state() const { return m_state; }
    int unlockOption() const { return m_unlockOption; }
    int timeLeft() const { return m_timeLeft; }
    /// The free list reroll, in seconds since the epoch (0: now).
    qint64 nextFreeListReroll() const { return m_nextFreeListReroll; }
    int automaticExtendType() const { return m_automaticExtendType; }
    const TPreyBonus& bonus() const { return m_bonus; }
    const TPreyMonster& monster() const { return m_monster; }
    const QList<TPreyMonster>& monsterList() const { return m_monsterList; }
    const QList<uint16_t>& raceList() const { return m_raceList; }

    /// The state setters of CIP 0x100ee2460 .. 0x100ee2c60.
    void setLocked(int unlockOption);
    void setInactive();
    void setActive(const TPreyBonus& bonus, const TPreyMonster& monster, int timeLeft);
    void setSelection(QList<TPreyMonster> list);
    void setSelectionChangeMonster(const TPreyBonus& bonus, QList<TPreyMonster> list);
    void setListSelection(QList<uint16_t> races);
    void setWildcardSelection(const TPreyBonus& bonus, QList<uint16_t> races);
    void setTimeLeft(int seconds);
    /// `seconds` from now, as the server sends it (0: free now).
    void setNextFreeListReroll(qint64 seconds);
    void setAutomaticExtendType(int type);

private:
    void setState(int state);
    void setBonus(const TPreyBonus& bonus);

    uint8_t m_preyId;
    int m_state = 0;
    int m_unlockOption = 0;
    int m_timeLeft = 0;
    qint64 m_nextFreeListReroll = 0;
    int m_automaticExtendType = 0;
    TPreyBonus m_bonus;
    TPreyMonster m_monster;
    QList<TPreyMonster> m_monsterList;
    QList<uint16_t> m_raceList;
};

/// The three prey slots and the prices (CIP: TPreyStorage, ctor 0x100ee56b0).
/// The dialog opens once the server sent every slot.
class TPreyStorage final : public cip::tibia::prey::TPreyStorage
{
    Q_OBJECT
public:
    static constexpr int SlotCount = 3;

    explicit TPreyStorage(QObject* parent = nullptr);

    TPreyData* slot(int preyId) const;
    const std::array<TPreyData*, SlotCount>& slots() const { return m_slots; }
    /// CIP vtable +0xb8: every slot arrived (the pending set is empty).
    bool allDataReceived() const { return m_pendingSlots.empty(); }
    /// CIP 0x100ee6080: a slot arrived.
    void markReceived(int preyId);
    void reset();

    int listRerollPrice() const { return m_listRerollPrice; }
    int bonusRerollPrice() const { return m_bonusRerollPrice; }
    int choosePreyPrice() const { return m_choosePreyPrice; }
    int preyWildcards() const { return m_preyWildcards; }
    bool bonusRerollPossible() const { return m_preyWildcards >= m_bonusRerollPrice; }
    bool choosePreyPossible() const { return m_preyWildcards >= m_choosePreyPrice; }
    /// CIP 0x100ee61f0 / 0x100ee6220 / 0x100ee6290.
    void setPrices(int listReroll, int bonusReroll, int choosePrey);
    /// CIP 0x100ee6300.
    void setPreyWildcards(int amount);

private:
    std::array<TPreyData*, SlotCount> m_slots{};
    std::set<int> m_pendingSlots;
    int m_listRerollPrice = 0;
    int m_bonusRerollPrice = 0;
    int m_choosePreyPrice = 0;
    int m_preyWildcards = 0;
};

}

namespace tibia::gamewindow {

/// One creature of a selection list (CIP: TListRerollEntryInfo).
class TListRerollEntryInfo final : public cip::tibia::gamewindow::TListRerollEntryInfo
{
    Q_OBJECT
public:
    explicit TListRerollEntryInfo(const prey::TPreyMonster& monster, QObject* parent = nullptr);
};

/// The bestiary races a slot can choose from (CIP: TPreyBestiarySelectionModel,
/// roles 0x101 "id" and 0x102 "name"), names capitalised per word.
class TPreyBestiarySelectionModel final : public cip::TPreyBestiarySelectionModel
{
    Q_OBJECT
public:
    enum ERole { IdRole = 0x101, NameRole = 0x102 };

    using cip::TPreyBestiarySelectionModel::TPreyBestiarySelectionModel;

    void setRaces(const QList<uint16_t>& races);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const override { return static_cast<int>(m_entries.size()); }

private:
    struct Entry
    {
        uint id = 0;
        QString name;
    };
    QList<Entry> m_entries;
};

/// CIP's proxy over TPreyBestiarySelectionModel: names containing the filter
/// (case-insensitive), sorted by lower-cased name.
class TPreyBestiarySelectionProxyModel final : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    void setNameFilter(const QString& filter);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    QString m_nameFilter;
};

/// One slot of SinglePreyView.qml (CIP: TPreyRenderInfo, ctor 0x100c627e0;
/// the slot at +0x30).
class TPreyRenderInfo final : public cip::tibia::gamewindow::TPreyRenderInfo
{
    Q_OBJECT
public:
    explicit TPreyRenderInfo(prey::TPreyData* data, QObject* parent = nullptr);

    uint preyId() const override;
    bool isLocked() const override;
    bool isInactive() const override;
    bool isActive() const override;
    bool isSelecting() const override;
    bool isGridSelecting() const override;
    bool isBestiarySelecting() const override;
    cip::tibia::qmlcomponents::enums::EPreyUnlockOption unlockOption() const override;
    QString timeLeft() const override;
    double timeLeftProgress() const override;
    QString bonusIcon() const override;
    QString bonusIconSmall() const override;
    QString bonusValueText() const override;
    uint bonusGrade() const override;
    uint maximumBonusGrade() const override;
    QString bonusGradeText() const override;
    QString bonusTypeText() const override;
    QString preyDescription() const override;
    QString monsterName() const override;
    uint monsterOutfitId() const override;
    QColor monsterHeadColor() const override;
    QColor monsterTorsoColor() const override;
    QColor monsterLegsColor() const override;
    QColor monsterDetailColor() const override;
    bool monsterFirstAddOn() const override;
    bool monsterSecondAddOn() const override;
    bool hasFreeListReroll() const override;
    QString timeToFreeListReroll() const override;
    double timeToFreeListRerollProgress() const override;
    QList<QObject*> listRerollData() const override { return m_listRerollData; }
    cip::tibia::qmlcomponents::enums::EPreyAutomaticExtendType automaticPreyExtendType() const override;
    QString automaticExtendTypeText() const override;
    QObject* bestiarySelectionModel() const override { return const_cast<TPreyBestiarySelectionProxyModel*>(&m_bestiaryProxy); }

    void rerollPreyBonusClicked() override;
    void listRerollClicked() override;
    void choosePreyClicked() override;
    void confirmGridPreyCreatureClicked(int Index) override;
    void confirmBestiaryPreyCreatureClicked(int MonsterRaceID) override;
    void unlockPermanentlyClicked() override;
    void unlockWithPermiumClicked() override;
    void autoExtendPreydClicked(cip::tibia::qmlcomponents::enums::EPreyAutomaticExtendType QMLAutoExtendType) override;
    void setMonsterRacesFilter(const QString& NameFilter) override;

Q_SIGNALS:
    void requestBonusReroll(uint preyId);
    void requestListReroll(uint preyId);
    void requestChoosePrey(uint preyId);
    void requestChangeAutoExtendType(uint preyId, int type);

private:
    /// Seconds until the free list reroll (0: free).
    qint64 secondsToFreeListReroll() const;
    void onMonsterListChanged();
    void onMonsterRaceListChanged();

    prey::TPreyData* m_data;
    TPreyBestiarySelectionModel m_bestiary;
    TPreyBestiarySelectionProxyModel m_bestiaryProxy;
};

/// The "Prey Creatures" tab, PreyPreyPage.qml (CIP: TPreyPreyPageController,
/// ctor 0x100c5d470).
class TPreyPreyPageController final : public cip::tibia::gamewindow::TPreyPreyPageController
{
    Q_OBJECT
public:
    TPreyPreyPageController(prey::TPreyStorage* storage, QObject* parent = nullptr);

    QList<QObject*> preyList() const override { return m_renderInfos; }
    QString preyWildcards() const override;
    QString listRerollPrice() const override;
    bool listRerollTooExpensive() const override;
    QString bonusRerollPrice() const override;
    bool bonusRerollTooExpensive() const override;
    QString choosePreyPrice() const override;
    bool choosePreyTooExpensive() const override;
    bool clientSettingSmoothFiltering() const override { return true; }

    /// CIP 0x100c5e930.
    void showDialogTab() override;
    /// CIP 0x100c615c0.
    void hideDialogTab() override;
    void getMorePreyWildcardsClicked() override;
    void onConfirmBonusReroll() override;
    void onConfirmListReroll() override;
    void onConfirmChoosePrey() override;
    void onConfirmEnableAutoRerollBonus() override;
    void onConfirmEnableLockPrey() override;

    /// The gold or wildcard balance changed.
    void onBalanceChanged();
    void closeMessages();

private:
    /// CIP 0x100c5f480 / 0x100c5fe00 / 0x100c60450 / 0x100c60dd0.
    void onRequestBonusReroll(uint preyId);
    void onRequestListReroll(uint preyId);
    void onRequestChoosePrey(uint preyId);
    void onRequestChangeAutoExtendType(uint preyId, int type);
    /// The wildcard question of a bonus reroll or a bestiary choice.
    void askToUseWildcards(TMessageDialogController* box, int price);
    void emitAll();

    prey::TPreyStorage* m_storage;
    QList<QObject*> m_renderInfos;
    uint m_pendingPreyId = 0;
    TMessageDialogController* m_bonusReroll;
    TMessageDialogController* m_listReroll;
    TMessageDialogController* m_choosePrey;
    TMessageDialogController* m_autoReroll;
    TMessageDialogController* m_lockPrey;
    QTimer m_preyListTimer;
    QTimer m_freeRerollTimer;
};

/// The prey dialog, PreyDialog.qml (CIP: TPreyDialogController, ctor
/// 0x100c5ba20).
class TPreyDialogController final : public TDialogController<cip::tibia::gamewindow::TPreyDialogController>
{
    Q_OBJECT
public:
    explicit TPreyDialogController(QObject* parent = nullptr);
    ~TPreyDialogController() override;

    prey::TPreyStorage* storage() const { return m_storage; }
    /// The gold or wildcard balance changed (onResourcesBalanceChange).
    void onBalanceChanged();
    /// Leaving the world.
    void clear();

    int preyWildcards() const override;

    /// CIP 0x100c5c560 (OPEN_PREY_DIALOG).
    void showDialog() override;
    /// CIP 0x100c5cbb0 (vtable 14).
    void closeDialog() override;
    void closeButtonClicked() override;
    void getMorePreyWildcardsClicked() override;
    void requestClose() override;
    /// CIP 0x100c5cdd0.
    void requestTabSwitch(EDialogTab NewTab) override;
    /// CIP 0x100c5c3e0.
    void onRequestShowMessageDialog(const QString& Message) override;

private:
    prey::TPreyStorage* m_storage;
    TPreyPreyPageController* m_page;
    TMessageDialogController* m_notOnMain;
    TMessageDialogController* m_info;
};

}
