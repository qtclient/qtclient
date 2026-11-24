#pragma once

#include "client/gamecore.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"

#include "container/containerciptypes.h"
#include "imbuing/imbuingciptypes.h"

#include <cip_gen.h>

#include <QJsonObject>
#include <QPointer>

#include <memory>
#include <vector>

namespace tibia {
namespace config {
class TSidebarOptions;
}
namespace game {
class IGameSession;
}
}

namespace tibia {
namespace gamewindow {

/// Rows for ImbuementTrackerWidget.qml (CIP: TImbuedObjectsModel).
class TImbuedObjectsModel final : public cip::TImbuedObjectsModel
{
    Q_OBJECT
public:
    /// CIP's role ids (0x101..0x11c).
    enum Role
    {
        TypeIdRole = Qt::UserRole + 1,
        CumulativeCountRole,
        TypeCountRole,
        CountStringRole,
        AlternativeSlotStringRole,
        LiquidTypeRole,
        HookDirectionRole,
        LootValueImageSourceUnderItemRole,
        LootValueImageSourceOverItemRole,
        ManagedContainerTooltipRole,
        UpgradeTierRole,
        InventoryPositionRole,
        SlotCountRole,
        Imbuement1NameRole,
        Imbuement1IconIdRole,
        Imbuement1RemainingTimeStringRole,
        Imbuement1RemainingTimeMsRole,
        Imbuement1VisualizationRole,
        Imbuement2NameRole,
        Imbuement2IconIdRole,
        Imbuement2RemainingTimeStringRole,
        Imbuement2RemainingTimeMsRole,
        Imbuement2VisualizationRole,
        Imbuement3NameRole,
        Imbuement3IconIdRole,
        Imbuement3RemainingTimeStringRole,
        Imbuement3RemainingTimeMsRole,
        Imbuement3VisualizationRole,
    };

    explicit TImbuedObjectsModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// CIP's simple vector model: the rows are the storage's objects (model reset).
    void setImbuedObjects(std::vector<imbuing::TImbuedObjectInformation> imbuedObjects);

    using Row = imbuing::TImbuedObjectInformation;

private:
    friend class TImbuedObjectsSortFilterProxyModel;
    friend class TImbuementTrackerWidgetController;

    std::vector<Row> m_rows;
    /// CIP +0x38: the timestamp of the last countdown step.
    qint64 m_lastUpdate = 0;
};

/// Filters imbued-object rows by sidebar show* flags (CIP: TImbuedObjectsSortFilterProxyModel).
class TImbuedObjectsSortFilterProxyModel final : public cip::TImbuedObjectsSortFilterProxyModel
{
    Q_OBJECT
public:
    /// CIP: the proxy owns its model and sorts column 0.
    explicit TImbuedObjectsSortFilterProxyModel(QObject* parent = nullptr);

    TImbuedObjectsModel* source() { return &m_source; }

    void setShowFlags(bool showAlmostGone, bool showUsed, bool showAlmostNew, bool showEmptySlots);

public Q_SLOTS:
    /// CIP slot — re-apply showAlmostGone / showUsed / showAlmostNew / showEmptySlots.
    void onOptionsChanged() override;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    TImbuedObjectsModel m_source;
    bool m_showAlmostGone = true;
    bool m_showUsed = true;
    bool m_showAlmostNew = true;
    bool m_showEmptySlots = true;
};

/// ImbuementTrackerWidget.qml (CIP: TImbuementTrackerWidgetController).
class TImbuementTrackerWidgetController final
    : public TSidebarWidgetController<cip::tibia::gamewindow::TImbuementTrackerWidgetController>
{
    Q_OBJECT
public:
    TImbuementTrackerWidgetController(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);
    /// CIP: empties the imbued objects model before it is destroyed.
    ~TImbuementTrackerWidgetController() override;

    /// CIP: follows the session's imbuement durations storage, and its sidebar
    /// options (applied now, again when they change or the character's
    /// configuration loads); the filters are applied again.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

    QObject* imbuedObjectsModel() const override;

    QQuickItem* gameWindowQuickItem() const override;

public Q_SLOTS:
    void onObjectClicked(int SlotIDRaw, int MouseButtonRaw, int KeyboardModifiersRaw) override;
    void optionsContextMenu() override;
    void onImbuedObjectsChanged() override;
    void saveWidgetOptions() override;
    void onCloseClicked() override;
    void onRequestClose() override;
    void onRequestMoveToSidebar(int TargetSidebar) override;
    void onSetDragActive(bool Active) override;
    void onSidebarDestroyed() override;
    void onLevelUpClicked() override;
    void setCursorToResizeVertical(bool Enabled) override;
    void startContentSizeChangedTimer();
    /// CIP (the sidebar widgets' frame update): once a second or more since the last
    /// step, the running imbuements count down by the time passed (not below 0) and
    /// their rows' time roles change.
    void updateWithTimestamp(qint64 timestampMilliseconds);

Q_SIGNALS:
    /// CIP sidebar chrome (TSidebarWidgetControllerBase) — qtclient NOTIFY deepen.
    void contentSizeChanged();
    void positionInSidebarChanged();
    void sidebarIdChanged();
    void widgetClosed();
    /// CIP chrome (shared_ptr / TSidebarID skipped) — qtclient NOTIFY deepen; drag/move
    /// forwarded via TSidebarWidget dynamic emit.
    void widgetDragChanged(uint sidebarId, bool active);
    void widgetWantsToMoveToSidebar(int targetSidebar, TSidebarWidget* widget);

private Q_SLOTS:
    /// CIP: the stored content height and maximized state are applied to the
    /// widget (host: with the stored filters).
    void onWidgetOptionsChanged();
    void onWidgetQuickItemDestroyed();

private:
    void applyFilterOptionsFromJson(const QJsonObject& json);
    void toggleShowFlag(bool& flag);

    TImbuedObjectsSortFilterProxyModel* m_proxy;
    TImbuedObjectsModel* m_model;
    std::vector<QMetaObject::Connection> m_sessionConnections;
    std::shared_ptr<game::IGameSession> m_gameSession;
    std::shared_ptr<imbuing::TImbuementDurationsStorage> m_imbuementDurationsStorage;
    QPointer<config::TSidebarOptions> m_options;
    bool m_showAlmostGone = true;
    bool m_showUsed = true;
    bool m_showAlmostNew = true;
    bool m_showEmptySlots = true;
};


}
}
