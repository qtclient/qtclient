#pragma once

// CIP-named gamewindow surfaces (promoted from cipwire).

#include "client/gamecore.h"
#include "gamewindow/dialogmanager.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"

#include <client/staticdata.h>
#include <cip_gen.h>

#include <QAbstractListModel>
#include <QEvent>
#include <QPointF>
#include <QPointer>
#include <QSortFilterProxyModel>
#include <QString>
#include <QTimer>
#include <QVariant>

#include <cstdint>
#include <vector>

class QQuickItem;
class QQmlEngine;

namespace tibia::gamewindow {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TActionBarController;
class TBattleListControllerStorage;

/// CIP TPreventAutoCloseEventFilter (eventFilter 0x100b74020).
class TPreventAutoCloseEventFilter final : public cip::tibia::gamewindow::TPreventAutoCloseEventFilter
{
    Q_OBJECT
public:
    explicit TPreventAutoCloseEventFilter(QObject* parent = nullptr);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
};

/// CIP TTargetSelectionEventFilter (eventFilter 0x100bdb630).
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

/// CIP TVipRenderInfo (ctor 0x100be9570) — VIP highlight timer.
class TVipRenderInfo final : public cip::tibia::gamewindow::TVipRenderInfo
{
    Q_OBJECT
public:
    explicit TVipRenderInfo(uint32_t characterId, QObject* parent = nullptr);

    uint32_t characterId() const { return m_characterId; }
    bool highlighted() const { return m_highlighted; }
    void startHighlight(int milliseconds = 1000);
    void clearHighlight();

public Q_SLOTS:
    void onHighlightedTimerTimeout();

Q_SIGNALS:
    void vipRenderInfoChanged(uint32_t characterId);

private:
    uint32_t m_characterId = 0;
    bool m_highlighted = false;
    QTimer m_timer;
};

/// CIP TWhiteBlacklistQmlEntry — one ignore-list row.
class TWhiteBlacklistQmlEntry final : public cip::tibia::gamewindow::TWhiteBlacklistQmlEntry
{
    Q_OBJECT
public:
    TWhiteBlacklistQmlEntry(QString name = {}, bool permanently = false, QObject* parent = nullptr);
};

/// CIP TTextCopyController (0x100c564c0) — cut/copy from QML text inputs.
class TTextCopyController final : public cip::tibia::gamewindow::TTextCopyController
{
    Q_OBJECT
public:
    explicit TTextCopyController(QObject* parent = nullptr);

    void cutFromTextInputQmlItemIfPossible(QObject* pTextInput) override;
    void copyFromTextInputQmlItemIfPossible(QObject* pTextInput) override;

private:
    void copyOrCut(QObject* textInput, bool cut);
};

class TSpritesheetDataObject final : public cip::tibia::gamewindow::TSpritesheetDataObject
{
    Q_OBJECT
public:
    explicit TSpritesheetDataObject(QString id = {}, QObject* parent = nullptr);
};

class TCyclopediaCreatureInfo final : public cip::tibia::gamewindow::TCyclopediaCreatureInfo
{
    Q_OBJECT
public:
    TCyclopediaCreatureInfo(int raceId, QString raceName, bool isUnknown, QObject* parent = nullptr);
};

class TCyclopediaCreatureOccurrencesInfo final : public cip::tibia::gamewindow::TCyclopediaCreatureOccurrencesInfo
{
    Q_OBJECT
public:
    explicit TCyclopediaCreatureOccurrencesInfo(QObject* parent = nullptr);
    void setSubArea(int id, const QString& name);
    void setCreatures(QList<QObject*> common, QList<QObject*> rare, QList<QObject*> dynamic);
};

class TCyclopediaMapSelectionInfoPassage final : public cip::tibia::gamewindow::TCyclopediaMapSelectionInfoPassage
{
    Q_OBJECT
public:
    explicit TCyclopediaMapSelectionInfoPassage(QObject* parent = nullptr);
    void setReachableCreatureOccurrences(QList<QObject*> occurrences);
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
    TQmlTargetWorld(QString worldName, QString detailedWorldName, bool premiumOnly, bool lockedForTransfer, QObject* parent = nullptr);
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

class TPassiveAbilityListDataModel final : public cip::tibia::gamewindow::TPassiveAbilityListDataModel
{
    Q_OBJECT
public:
    enum Role { NameRole = Qt::UserRole + 1, IdRole, EnumIdRole, IconUrlRole, UsableRole };

    struct Entry {
        int id = 0;
        QString name;
        QString iconUrl;
        bool usableByCurrentVocation = true;
    };

    explicit TPassiveAbilityListDataModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void setEntries(std::vector<Entry> entries);

private:
    std::vector<Entry> m_entries;
};

class TPassiveAbilityListDataModelSortFilterProxy final : public cip::tibia::gamewindow::TPassiveAbilityListDataModelSortFilterProxy
{
    Q_OBJECT
public:
    explicit TPassiveAbilityListDataModelSortFilterProxy(QObject* parent = nullptr);

    QVariant getDataWithRoleName(int RowIndex, const QByteArray& RoleName) override;
    void onPlayerDataChanged() override;
    void setNameFilter(const QString& filter);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QString m_nameFilter;
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

class TVesselInformation final : public cip::tibia::gamewindow::TVesselInformation
{
    Q_OBJECT
public:
    using cip::tibia::gamewindow::TVesselInformation::TVesselInformation;
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
    enum Role { NameRole = Qt::UserRole + 1, CountRole, ObjectIdRole };

    struct Entry {
        QString name;
        int count = 0;
        int objectId = 0;
    };

    explicit THouseItemModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void setEntries(std::vector<Entry> entries);

private:
    std::vector<Entry> m_entries;
};

class TBattleListDataModelSortFilterProxy final : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit TBattleListDataModelSortFilterProxy(QObject* parent = nullptr);

protected:
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
};

class TPartyHuntMembersListModelSortFilterProxy final : public cip::tibia::gamewindow::TPartyHuntMembersListModelSortFilterProxy
{
    Q_OBJECT
public:
    explicit TPartyHuntMembersListModelSortFilterProxy(QObject* parent = nullptr);

protected:
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
};

class TBattleListGameActionHandler final : public cip::tibia::gamewindow::TBattleListGameActionHandler
{
    Q_OBJECT
public:
    explicit TBattleListGameActionHandler(TBattleListControllerStorage* storage, QObject* parent = nullptr);

public Q_SLOTS:
    void attackFirstTarget() override;
    void attackNextOrPreviousTarget(bool Previous) override;
    void handleRemoveAllTutorialMarkersGameAction() override;

private:
    QPointer<TBattleListControllerStorage> m_storage;
};

class TAssignActionDialogControllerBase : public cip::tibia::gamewindow::TAssignActionDialogControllerBase
{
    Q_OBJECT
public:
    explicit TAssignActionDialogControllerBase(QObject* parent = nullptr);
    void bindActionButton(TActionBarController* bar, int buttonId);

public Q_SLOTS:
    void onCloseClicked() override;
};

class TDialogControllerWithFocusHandlingBase : public cip::tibia::gamewindow::TDialogControllerWithFocusHandlingBase
{
    Q_OBJECT
public:
    explicit TDialogControllerWithFocusHandlingBase(QObject* parent = nullptr);

public Q_SLOTS:
    void takeFocus() override;
    void releaseFocus() override;
};

class TSidebarPanelControllerBase : public cip::tibia::gamewindow::TSidebarPanelControllerBase
{
    Q_OBJECT
public:
    explicit TSidebarPanelControllerBase(QObject* parent = nullptr);

public Q_SLOTS:
    void onSidebarDestroyed() override;
    void onPanelQuickItemDestroyed() override;
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

    QQuickItem* gameWindowQuickItem() const override;
    uint parentSidebarId() const override;

public Q_SLOTS:
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
};

class TInspectObjectDialogController final : public TDialogController<cip::tibia::gamewindow::TInspectObjectDialogController>
{
    Q_OBJECT
public:
    explicit TInspectObjectDialogController(QObject* parent = nullptr);
    static TInspectObjectDialogController* instance();

public Q_SLOTS:
    void closeDialog() override;
    void okButtonClicked() override;
    void copyToClipboarClicked() override;

private:
    void showObject(const ItemInspectionData& data);
    static TInspectObjectDialogController* s_instance;
    std::vector<client::gamecore::Hook> m_hooks;
};

class TInspectPlayerDialogController final : public TDialogController<cip::tibia::gamewindow::TInspectPlayerDialogController>
{
    Q_OBJECT
public:
    explicit TInspectPlayerDialogController(QObject* parent = nullptr);
    static TInspectPlayerDialogController* instance();

public Q_SLOTS:
    void closeDialog() override;
    void okButtonClicked() override;
    void copyToClipboarClicked(const QString& Text) override;

private:
    void showPlayer(const CyclopediaCharacterInspection& data);
    static TInspectPlayerDialogController* s_instance;
    std::vector<client::gamecore::Hook> m_hooks;
};

// CipSoft DepotSearchWidget lives in depotsearchwidgetcontroller.h/.cpp

class TChatChannelFilterController final : public cip::tibia::gamewindow::TChatChannelFilterController
{
    Q_OBJECT
public:
    explicit TChatChannelFilterController(QObject* parent = nullptr);

    void setChannelModel(QAbstractItemModel* source);
    void syncFromChannel(bool canSubscribe, bool subscribed, bool writesLocal, bool canClose, const QString& name);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
};

class TChatChannelTabRenderInfo final : public cip::tibia::gamewindow::TChatChannelTabRenderInfo
{
    Q_OBJECT
public:
    explicit TChatChannelTabRenderInfo(QObject* parent = nullptr);

    void startFlash(int milliseconds = 1000);
    void stopFlash();
    bool flashing() const { return m_timer.isActive(); }

public Q_SLOTS:
    void onFlashTimerTimeout();

Q_SIGNALS:
    void flashFinished();

private:
    QTimer m_timer;
};

class TChatTutorialMarkerRenderInfo final : public cip::tibia::gamewindow::TChatTutorialMarkerRenderInfo
{
    Q_OBJECT
public:
    TChatTutorialMarkerRenderInfo(int markerId, int startIndex, int length, QObject* parent = nullptr);
};

}
