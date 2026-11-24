#pragma once

#include <map>
#include <vector>

// CIP-named gamewindow surfaces (promoted from cipwire).

#include "client/gamecore.h"
#include "gamewindow/anonymousciptypes.h"
#include "gamewindow/dialogmanager.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"
#include "gamewindow/uifocusmanager.h"

#include <client/staticdata.h>
#include <cip_gen.h>

#include <QAbstractListModel>
#include <QEvent>
#include <QHash>
#include <QPointF>
#include <QPointer>
#include <QSortFilterProxyModel>
#include <QString>
#include <QTimer>
#include <QVariant>

#include <cstdint>
#include <memory>
#include <vector>

namespace tibia::game {
class IGameSession;
}

class QQuickItem;
class QQmlEngine;

namespace tibia::config {
class TClientOptions;
}

namespace tibia::store {
struct TChildTransactionDetails;
}

namespace tibia {
namespace gamewindow {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TActionBarController;
class TBattleListControllerStorage;

/// CIP TPreventAutoCloseEventFilter (eventFilter).
class TPreventAutoCloseEventFilter final : public cip::tibia::gamewindow::TPreventAutoCloseEventFilter
{
    Q_OBJECT
public:
    explicit TPreventAutoCloseEventFilter(QObject* parent = nullptr);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
};

/// CIP TTargetSelectionEventFilter (eventFilter).
class TTargetSelectionEventFilter final : public cip::tibia::gamewindow::TTargetSelectionEventFilter
{
    Q_OBJECT
public:
    explicit TTargetSelectionEventFilter(QQuickItem* root, QObject* parent = nullptr);

    void setSelecting(bool selecting);
    bool filtering() const { return m_filtering; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void emitTargetSelected(const QPointF& scenePosition);

    QPointer<QQuickItem> m_root;
    Qt::MouseButtons m_initialButtons;
    bool m_filtering = false;
};

/// CIP TWhiteBlacklistQmlEntry — one ignore-list row.
class TWhiteBlacklistQmlEntry final : public cip::tibia::gamewindow::TWhiteBlacklistQmlEntry
{
    Q_OBJECT
public:
    TWhiteBlacklistQmlEntry(QString name = {}, bool permanently = false, QObject* parent = nullptr);
};

/// CIP TTextCopyController — cut/copy from QML text inputs.
class TTextCopyController final : public cip::tibia::gamewindow::TTextCopyController
{
    Q_OBJECT
public:
    explicit TTextCopyController(QObject* parent = nullptr);

    /// CIP: makes the process-wide controller (a new one replaces the old)
    /// and offers it to the engine's QML as `tibiaTextCopyController`.
    /// Host: it goes with the engine (Qt objects must not outlive the
    /// application).
    static void createInstance(QQmlEngine* Engine);

    void cutFromTextInputQmlItemIfPossible(QObject* pTextInput) override;
    void copyFromTextInputQmlItemIfPossible(QObject* pTextInput) override;
};

class TSpritesheetDataObject final : public cip::tibia::gamewindow::TSpritesheetDataObject
{
    Q_OBJECT
public:
    explicit TSpritesheetDataObject(QString id = {}, QObject* parent = nullptr);

    /// CIP: the id is replaced without idChanged.
    void setId(const QString& v) override;
};

class TCyclopediaCreatureInfo final : public cip::tibia::gamewindow::TCyclopediaCreatureInfo
{
    Q_OBJECT
public:
    TCyclopediaCreatureInfo(int raceId, QString raceName, bool isUnknown, QObject* parent = nullptr);
};

/// A creature of a subarea's occurrence lists.
struct TCyclopediaCreatureData
{
    uint16_t raceId = 0;
    QString raceName;
    bool isUnknown = false;
};

/// A subarea and its creatures by occurrence: 0 common, 1 rare, 2 dynamic.
struct TCyclopediaCreatureOccurrencesData
{
    uint16_t subAreaId = 0;
    QString subAreaName;
    std::map<int, std::vector<TCyclopediaCreatureData>> creatures;
};

class TCyclopediaCreatureOccurrencesInfo final : public cip::tibia::gamewindow::TCyclopediaCreatureOccurrencesInfo
{
    Q_OBJECT
public:
    explicit TCyclopediaCreatureOccurrencesInfo(QObject* parent = nullptr);
    /// One TCyclopediaCreatureInfo child per creature, listed by occurrence.
    explicit TCyclopediaCreatureOccurrencesInfo(const TCyclopediaCreatureOccurrencesData& data, QObject* parent = nullptr);
    void setSubArea(int id, const QString& name);
    void setCreatures(QList<QObject*> common, QList<QObject*> rare, QList<QObject*> dynamic);
};

class TQMLCharacterData final : public cip::tibia::gamewindow::TQMLCharacterData
{
    Q_OBJECT
public:
    explicit TQMLCharacterData(QObject* parent = nullptr);
    void setCharacter(const QString& name, const QString& vocation, int level, const QString& worldType, QObject* outfit = nullptr);
};

class TQmlChildTransaction final : public cip::tibia::gamewindow::TQmlChildTransaction
{
    Q_OBJECT
public:
    TQmlChildTransaction(QString date, QString description, EMarketTransactionCurrency currency, qlonglong balanceChange,
                         QObject* parent = nullptr);
    /// One part of a transaction sold in parts: its date (with seconds), description and balance
    /// change, in gold for a type 1 part, else in Tibia Coins.
    explicit TQmlChildTransaction(const store::TChildTransactionDetails& child, QObject* parent = nullptr);
};

class TQmlPossibleMainChar final : public cip::tibia::gamewindow::TQmlPossibleMainChar
{
    Q_OBJECT
public:
    TQmlPossibleMainChar(QString name, QString world, QString vocation, int level, QObject* parent = nullptr);
};

class TQmlTargetWorld final : public cip::tibia::gamewindow::TQmlTargetWorld
{
    Q_OBJECT
public:
    TQmlTargetWorld(QString worldName, bool premiumOnly, bool lockedForTransfer, int pvpType, QObject* parent = nullptr);

    /// The name with the PvP type, and whether the world is premium only
    /// and/or locked for transfers.
    QString detailedWorldName() const override;

private:
    int m_pvpType = 0;
};

class TQmlWorldTransferCondition final : public cip::tibia::gamewindow::TQmlWorldTransferCondition
{
    Q_OBJECT
public:
    TQmlWorldTransferCondition(bool meets, QString text, QObject* parent = nullptr);
};

class TAccountCharactersModel final : public cip::tibia::gamewindow::TAccountCharactersModel
{
    Q_OBJECT
public:
    enum Role { NameRole = Qt::UserRole + 1, LevelRole, VocationRole, WorldRole };

    explicit TAccountCharactersModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void setCharacters(std::vector<TQMLCharacterData*> rows);

private:
    std::vector<TQMLCharacterData*> m_rows;
};

class TLargePerkInformation final : public cip::tibia::gamewindow::TLargePerkInformation
{
    Q_OBJECT
public:
    using cip::tibia::gamewindow::TLargePerkInformation::TLargePerkInformation;
};

class TTileInformation final : public cip::tibia::gamewindow::TTileInformation
{
    Q_OBJECT
public:
    using cip::tibia::gamewindow::TTileInformation::TTileInformation;
};

class TSimpleNameAndCountModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role { NameRole = Qt::UserRole + 1, CountRole };

    struct Entry {
        QString name;
        int count = 0;
    };

    explicit TSimpleNameAndCountModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void setEntries(std::vector<Entry> entries);

private:
    std::vector<Entry> m_entries;
};

class THouseItemModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role { AppearanceTypeIdRole = Qt::UserRole + 1, NameRole, CountRole };

    /// One house item: its appearance, name and count.
    struct Entry {
        quint16 appearanceTypeId = 0;
        QString name;
        uint count = 0;
    };

    explicit THouseItemModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void setEntries(std::vector<Entry> entries);

private:
    std::vector<Entry> m_entries;
};

class TPartyHuntMembersListModelSortFilterProxy final : public cip::tibia::gamewindow::TPartyHuntMembersListModelSortFilterProxy
{
    Q_OBJECT
public:
    explicit TPartyHuntMembersListModelSortFilterProxy(QObject* parent = nullptr);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
};

/// CipSoft TDialogControllerBase signal surface (abort / dialogClosed).
/// Live dialogs still use cip::…::TDialogControllerBase via TDialogController<>;
/// qtclient twin owns the moc NOTIFY names for inventory (TDialog::close emits dialogClosed).
class TDialogControllerBase : public cip::tibia::gamewindow::TDialogControllerBase
{
    Q_OBJECT
public:
    explicit TDialogControllerBase(QObject* parent = nullptr);

Q_SIGNALS:
    void abort();
    void dialogClosed();
};

class TSidebarWidgetControllerBase : public cip::tibia::gamewindow::TSidebarWidgetControllerBase
{
    Q_OBJECT
public:
    TSidebarWidgetControllerBase(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);

    QQuickItem* gameWindowQuickItem() const override;
    uint parentSidebarId() const override;

public Q_SLOTS:
    void onCloseClicked() override;
    void onRequestClose() override;
    void onRequestMoveToSidebar(int TargetSidebar) override;
    void onSetDragActive(bool Active) override;
    void onSidebarDestroyed() override;
    void setCursorToResizeVertical(bool Enabled) override;

    TSidebarWidget* widget() const { return m_widget; }

private:
    TSidebarWidget* m_widget = nullptr;
};

class TSidebarWidgetControllerWithFocusHandling : public cip::tibia::gamewindow::TSidebarWidgetControllerWithFocusHandling
{
    Q_OBJECT
public:
    TSidebarWidgetControllerWithFocusHandling(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);
    ~TSidebarWidgetControllerWithFocusHandling() override;

    QQuickItem* gameWindowQuickItem() const override;
    uint parentSidebarId() const override;
    /// CIP: the focus manager's focus.
    bool hasFocus() const override;

public Q_SLOTS:
    /// CIP: the widget's focus is its focus manager's.
    void takeFocus() override;
    void releaseFocus() override;
    void onCloseClicked() override;
    void onRequestClose() override;
    void onRequestMoveToSidebar(int TargetSidebar) override;
    void onSetDragActive(bool Active) override;
    void onSidebarDestroyed() override;
    void setCursorToResizeVertical(bool Enabled) override;

    TSidebarWidget* widget() const { return m_widget; }

private:
    TSidebarWidget* m_widget = nullptr;
    TUiFocusManager m_focus;
};

class TInspectObjectDialogController final : public TDialogController<cip::tibia::gamewindow::TInspectObjectDialogController>
{
    Q_OBJECT
public:
    TInspectObjectDialogController(config::TClientOptions* ClientOptions, QQmlEngine* Engine, QQuickItem* DialogParentItem,
                                   QObject* parent = nullptr);
    static TInspectObjectDialogController* instance();
    bool clientSettingSmoothFiltering() const override;
    /// CIP connector: the dialog closes and keeps the session.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

public Q_SLOTS:
    /// CIP: INSPECT_OBJECT (with a game session) shows the next inspection
    /// result and asks for it: a player trade item through the trade handler,
    /// anything else as an InspectObject message of its location.
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction);
    void closeDialog() override;
    void okButtonClicked() override;
    void copyToClipboarClicked() override;
    void openWeaponProficiencyDialogClicked() override;

private:
    void showObject(const ItemInspectionData& data);
    static TInspectObjectDialogController* s_instance;
    /// The inspection results reach showObject (CIP +0x40).
    QMetaObject::Connection m_inspectionConnection;
    std::shared_ptr<game::IGameSession> m_gameSession;
    config::TClientOptions* m_clientOptions = nullptr;
};

class TInspectPlayerDialogController final : public TDialogController<cip::tibia::gamewindow::TInspectPlayerDialogController>
{
    Q_OBJECT
public:
    TInspectPlayerDialogController(config::TClientOptions* ClientOptions, QQmlEngine* Engine, QQuickItem* DialogParentItem,
                                   QObject* parent = nullptr);
    static TInspectPlayerDialogController* instance();
    bool clientSettingSmoothFiltering() const override;
    /// CIP connector: the dialog closes; the session's game handler opens it
    /// with an inspected player.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

public Q_SLOTS:
    /// Also drops the pointing hand and the rows.
    void closeDialog() override;
    void okButtonClicked() override;
    /// The Cyclopedia's character tab; closes.
    void openCharacterInfoClicked() override;
    void copyToClipboarClicked(const QString& Text) override;
    /// The inspected player's skill wheel.
    void openSkillWheelDialog() override;
    void openWeaponProficiencyDialogClicked(uint AppearanceTypeID) override;

private:
    void showPlayer(const CyclopediaCharacterInspection& data);
    static TInspectPlayerDialogController* s_instance;
    std::shared_ptr<game::IGameSession> m_gameSession;
    std::vector<QMetaObject::Connection> m_gameSessionConnections;
    uint32_t m_inspectedPlayerId = 0;
    config::TClientOptions* m_clientOptions = nullptr;
};

// CipSoft DepotSearchWidget lives in depotsearchwidgetcontroller.h.cpp

class TChatTutorialMarkerRenderInfo final : public cip::tibia::gamewindow::TChatTutorialMarkerRenderInfo
{
    Q_OBJECT
public:
    TChatTutorialMarkerRenderInfo(int markerId, int startIndex, int length, QObject* parent = nullptr);
};

/// CIP QML DTOs / list payloads (pin units); deepen fields from units as needed.
class TAccountCharacterListData final : public ::tibia::CipTyped { public: TAccountCharacterListData() = default; };
class TInspectObjectQMLData final : public ::tibia::CipTyped { public: TInspectObjectQMLData() = default; };
/// An outfit as QML shows it (CIP: TOutfitQMLInformation): the look type, the four colour indices as
/// colours and the addons.
class TOutfitQMLInformation final : public cip::tibia::gamewindow::TOutfitQMLInformation
{
    Q_OBJECT
public:
    TOutfitQMLInformation(int id, int head, int torso, int legs, int detail, int addons, QObject* parent = nullptr);
};

}
}
