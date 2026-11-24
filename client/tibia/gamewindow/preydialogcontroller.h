#pragma once

#include "gamewindow/messagedialogcontroller.h"
#include "prey/preystorage.h"
#include "input/gameactionptr.h"
#include "qtconnectionmanager.h"

#include <cip_gen.h>

#include <QList>
#include <QPointer>
#include <QSortFilterProxyModel>
#include <QString>
#include <QTimer>

#include <array>
#include <cstdint>
#include <memory>
#include <set>
#include <vector>

namespace tibia {
namespace config {
class TClientOptions;
}
namespace game {
class IGameSession;
}
}

namespace tibia {
namespace gamewindow {

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

    /// Inserted and removed rows change the count.
    explicit TPreyBestiarySelectionModel(QObject* parent = nullptr);

    void setRaces(const QList<uint16_t>& races);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const override;

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
    const QString& nameFilter() const { return m_nameFilter; }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    QString m_nameFilter;
};

/// One slot of SinglePreyView.qml (CIP: TPreyRenderInfo, ctor ;
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
    QString m_monsterRacesFilter;
    TPreyBestiarySelectionModel m_bestiary;
    TPreyBestiarySelectionProxyModel m_bestiaryProxy;
};

/// The "Prey Creatures" tab, PreyPreyPage.qml (CIP: TPreyPreyPageController,
/// ctor).
class TPreyPreyPageController final : public cip::tibia::gamewindow::TPreyPreyPageController
{
    Q_OBJECT
public:
    /// CIP: the engine and dialog parent item are its message dialogs' (they
    /// are shown through the dialog stack here).
    TPreyPreyPageController(config::TClientOptions* ClientOptions, QQmlEngine* Engine, QQuickItem* DialogParentItem,
                            QObject* parent = nullptr);
    /// CIP: the slot render infos go with the page.
    ~TPreyPreyPageController() override;

    QList<QObject*> preyList() const override { return m_renderInfos; }
    QString preyWildcards() const override;
    QString listRerollPrice() const override;
    bool listRerollTooExpensive() const override;
    QString bonusRerollPrice() const override;
    bool bonusRerollTooExpensive() const override;
    QString choosePreyPrice() const override;
    bool choosePreyTooExpensive() const override;
    bool clientSettingSmoothFiltering() const override;

    /// CIP .
    void showDialogTab() override;
    /// CIP .
    void hideDialogTab() override;
    void getMorePreyWildcardsClicked() override;
    void onConfirmBonusReroll() override;
    void onConfirmListReroll() override;
    void onConfirmChoosePrey() override;
    void onConfirmEnableAutoRerollBonus() override;
    void onConfirmEnableLockPrey() override;

    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

    /// The gold or wildcard balance changed.
    void onBalanceChanged();
    /// The free list reroll countdowns tick.
    void onAdvanceTimeUntilFreeListReroll();
    void closeMessages();

private:
    /// CIP / /.
    void onRequestBonusReroll(uint preyId);
    void onRequestListReroll(uint preyId);
    void onRequestChoosePrey(uint preyId);
    void onRequestChangeAutoExtendType(uint preyId, int type);
    /// The wildcard question of a bonus reroll or a bestiary choice.
    void askToUseWildcards(TMessageDialogController* box, int price, bool possible);
    void emitAll();

    std::shared_ptr<game::IGameSession> m_gameSession;
    QPointer<prey::TPreyStorage> m_storage;
    shared::TQtConnectionManager m_gameSessionConnections;
    config::TClientOptions* m_clientOptions;
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
/// ).
class TPreyDialogController final : public TDialogController<cip::tibia::gamewindow::TPreyDialogController>
{
    Q_OBJECT
public:
    TPreyDialogController(config::TClientOptions* ClientOptions, QQmlEngine* Engine, QQuickItem* DialogParentItem,
                          QObject* parent = nullptr);
    ~TPreyDialogController() override;

    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

    /// The session's prey storage, or null.
    prey::TPreyStorage* storage() const { return m_storage; }
    /// The gold or wildcard balance changed (onResourcesBalanceChange).
    void onBalanceChanged();

    int preyWildcards() const override;

    /// CIP (OPEN_PREY_DIALOG).
    void showDialog() override;

public Q_SLOTS:
    /// CIP: OPEN_PREY_DIALOG shows the dialog (connected to the game window's
    /// publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction);

public:
    /// CIP (vtable 14).
    void closeDialog() override;
    void closeButtonClicked() override;
    void getMorePreyWildcardsClicked() override;
    void requestClose() override;
    /// CIP .
    void requestTabSwitch(EDialogTab NewTab) override;
    /// CIP .
    void onRequestShowMessageDialog(const QString& Message) override;

private:
    std::shared_ptr<game::IGameSession> m_gameSession;
    shared::TQtConnectionManager m_gameSessionConnections;
    QPointer<prey::TPreyStorage> m_storage;
    TPreyPreyPageController* m_page;
    TMessageDialogController* m_notOnMain;
    TMessageDialogController* m_info;
};

}
}
