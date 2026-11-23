#include "imbuementtrackerwidgetcontroller.h"

#include "gamewindow/appearanceinstanceroledata.h"

#include "config/sidebaroptions.h"
#include "game/gamesessionbase.h"
#include "gamewindow/contextmenucontroller.h"
#include "qmlcomponents/texthelper.h"

#include <client/staticdata.h>

#include "clockservice.h"

#include <QQuickWindow>

#include <algorithm>

namespace tibia::gamewindow {

namespace {

using Viz = cip::tibia::qmlcomponents::enums::EVisualizationType;
constexpr auto OptionsKey = "imbuementTrackerWidgetOptions";

/// CIP: the remaining time, with the ultra short form as the elided variant when it differs.
QString remainingTimeString(const qint64 milliseconds)
{
    const QString full = qmlcomponents::shortDurationString(milliseconds / 1000);
    const QString shortForm = qmlcomponents::ultraShortDurationString(milliseconds / 1000);
    if (full == shortForm)
        return full;
    return QStringList{ full, shortForm }.join(QChar(0x9c));
}

/// CIP: three hours or more almost new, one hour or more used, less almost gone.
Viz visualization(const qint64 milliseconds)
{
    if (milliseconds < 3600000)
        return Viz::AlmostGone;
    return milliseconds < 10800000 ? Viz::Used : Viz::AlmostNew;
}

bool showFlagEnabled(const int visualization, const bool showAlmostGone, const bool showUsed,
                     const bool showAlmostNew)
{
    switch (static_cast<Viz>(visualization)) {
    case Viz::AlmostGone:
        return showAlmostGone;
    case Viz::Used:
        return showUsed;
    case Viz::AlmostNew:
        return showAlmostNew;
    }
    return true;
}

}

TImbuedObjectsModel::TImbuedObjectsModel(QObject* parent)
    : cip::TImbuedObjectsModel(parent)
{}

int TImbuedObjectsModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

namespace {

/// The object roles without options or prices, the count in its short form.
QVariant objectData(const TImbuedObjectsModel::Row& row, const EAppearanceInstanceRole role)
{
    return appearanceInstanceRoleData(row.object, role, TAppearanceInstanceRoleSources{}, true);
}

}

namespace {

/// The slot `index` of a row; an empty slot past the object's slots.
const imbuing::TImbuementSlotInformation& slotOf(const TImbuedObjectsModel::Row& row, const size_t index)
{
    static const imbuing::TImbuementSlotInformation empty;
    return index < row.slots.size() ? row.slots[index] : empty;
}

}

QVariant TImbuedObjectsModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= int(m_rows.size()))
        return {};
    const Row& row = m_rows[size_t(index.row())];
    if (role >= Imbuement1NameRole && role <= Imbuement3VisualizationRole) {
        const imbuing::TImbuementSlotInformation& slot = slotOf(row, size_t((role - Imbuement1NameRole) / 5));
        switch ((role - Imbuement1NameRole) % 5) {
        case 0:
            return slot.name;
        case 1:
            return int(slot.iconId);
        case 2:
            return remainingTimeString(slot.remainingMilliseconds);
        case 3:
            return slot.remainingMilliseconds;
        default:
            return QVariant::fromValue(visualization(slot.remainingMilliseconds));
        }
    }
    switch (role) {
    case InventoryPositionRole:
        return int(row.inventorySlot);
    case SlotCountRole:
        return int(row.slots.size());
    case TypeIdRole: return objectData(row, EAppearanceInstanceRole::TypeId);
    case CumulativeCountRole: return objectData(row, EAppearanceInstanceRole::CumulativeCount);
    case TypeCountRole: return objectData(row, EAppearanceInstanceRole::TypeCount);
    case CountStringRole: return objectData(row, EAppearanceInstanceRole::CountString);
    case AlternativeSlotStringRole: return objectData(row, EAppearanceInstanceRole::AlternativeSlotString);
    case LiquidTypeRole: return objectData(row, EAppearanceInstanceRole::LiquidType);
    case HookDirectionRole: return objectData(row, EAppearanceInstanceRole::HookDirection);
    case LootValueImageSourceUnderItemRole: return objectData(row, EAppearanceInstanceRole::LootValueImageSourceUnderItem);
    case LootValueImageSourceOverItemRole: return objectData(row, EAppearanceInstanceRole::LootValueImageSourceOverItem);
    case ManagedContainerTooltipRole: return objectData(row, EAppearanceInstanceRole::ManagedContainerTooltip);
    case UpgradeTierRole: return objectData(row, EAppearanceInstanceRole::UpgradeTier);
    default:
        return {};
    }
}

QHash<int, QByteArray> TImbuedObjectsModel::roleNames() const
{
    return {
        { TypeIdRole, "typeID" },
        { CumulativeCountRole, "cumulativeCount" },
        { TypeCountRole, "typeCount" },
        { CountStringRole, "countString" },
        { AlternativeSlotStringRole, "alternativeSlotString" },
        { LiquidTypeRole, "liquideType" },
        { HookDirectionRole, "hookDirection" },
        { LootValueImageSourceUnderItemRole, "lootvalueImageSourceUnderItem" },
        { LootValueImageSourceOverItemRole, "lootvalueImageSourceOverItem" },
        { ManagedContainerTooltipRole, "managedContainerTooltip" },
        { UpgradeTierRole, "upgradeTier" },
        { InventoryPositionRole, "inventoryPosition" },
        { SlotCountRole, "slotCount" },
        { Imbuement1NameRole, "imbuement1Name" },
        { Imbuement1IconIdRole, "imbuement1IconID" },
        { Imbuement1RemainingTimeStringRole, "imbuement1RemainingTimeString" },
        { Imbuement1RemainingTimeMsRole, "imbuement1RemainingTimeMs" },
        { Imbuement1VisualizationRole, "imbuement1Visualization" },
        { Imbuement2NameRole, "imbuement2Name" },
        { Imbuement2IconIdRole, "imbuement2IconID" },
        { Imbuement2RemainingTimeStringRole, "imbuement2RemainingTimeString" },
        { Imbuement2RemainingTimeMsRole, "imbuement2RemainingTimeMs" },
        { Imbuement2VisualizationRole, "imbuement2Visualization" },
        { Imbuement3NameRole, "imbuement3Name" },
        { Imbuement3IconIdRole, "imbuement3IconID" },
        { Imbuement3RemainingTimeStringRole, "imbuement3RemainingTimeString" },
        { Imbuement3RemainingTimeMsRole, "imbuement3RemainingTimeMs" },
        { Imbuement3VisualizationRole, "imbuement3Visualization" },
    };
}

void TImbuedObjectsModel::setImbuedObjects(std::vector<imbuing::TImbuedObjectInformation> imbuedObjects)
{
    beginResetModel();
    m_rows = std::move(imbuedObjects);
    endResetModel();
}

TImbuedObjectsSortFilterProxyModel::TImbuedObjectsSortFilterProxyModel(QObject* parent)
    : cip::TImbuedObjectsSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    sort(0);
    setSourceModel(&m_source);
}

void TImbuedObjectsSortFilterProxyModel::setShowFlags(const bool showAlmostGone, const bool showUsed,
                                                      const bool showAlmostNew, const bool showEmptySlots)
{
    m_showAlmostGone = showAlmostGone;
    m_showUsed = showUsed;
    m_showAlmostNew = showAlmostNew;
    m_showEmptySlots = showEmptySlots;
    onOptionsChanged();
}

void TImbuedObjectsSortFilterProxyModel::onOptionsChanged()
{
    beginFilterChange();
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

bool TImbuedObjectsSortFilterProxyModel::filterAcceptsRow(const int sourceRow, const QModelIndex&) const
{
    if (sourceRow < 0 || sourceRow >= m_source.rowCount())
        return false;
    const TImbuedObjectsModel::Row& row = m_source.m_rows[size_t(sourceRow)];
    const auto imbued = [](const imbuing::TImbuementSlotInformation& slot) { return slot.imbued; };
    if (std::none_of(row.slots.begin(), row.slots.end(), imbued))
        return m_showEmptySlots;
    for (const imbuing::TImbuementSlotInformation& slot : row.slots) {
        if (!slot.imbued)
            continue;
        if (showFlagEnabled(static_cast<int>(visualization(slot.remainingMilliseconds)), m_showAlmostGone, m_showUsed, m_showAlmostNew))
            return true;
    }
    return false;
}

TImbuementTrackerWidgetController::TImbuementTrackerWidgetController(QQuickItem* gameWindowItem, QQmlEngine* engine,
                                                                     QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
    , m_proxy(new TImbuedObjectsSortFilterProxyModel(this))
    , m_model(m_proxy->source())
{
    m_imbuedObjectsModel = m_proxy;
    widget()->createItem(QStringLiteral("ImbuementTrackerWidget.qml"));
    if (QQuickItem* item = quickItem())
        connect(item, &QObject::destroyed, this, &TImbuementTrackerWidgetController::onWidgetQuickItemDestroyed);
    // Host: the frame clock starts at the wall clock, so the countdown starts from now.
    m_model->m_lastUpdate = shared::TClockService::frameMillis();
}

void TImbuementTrackerWidgetController::updateWithTimestamp(const qint64 timestampMilliseconds)
{
    TImbuedObjectsModel& model = *m_model;
    const qint64 elapsed = timestampMilliseconds - model.m_lastUpdate;
    if (elapsed < 1000)
        return;
    model.m_lastUpdate = timestampMilliseconds;
    for (size_t row = 0; row < model.m_rows.size(); ++row) {
        TImbuedObjectsModel::Row& entry = model.m_rows[row];
        bool changed = false;
        for (imbuing::TImbuementSlotInformation& slot : entry.slots) {
            if (!slot.imbued || !slot.decaying || slot.remainingMilliseconds <= 0)
                continue;
            slot.remainingMilliseconds = std::max<qint64>(0, slot.remainingMilliseconds - elapsed);
            changed = true;
        }
        if (!changed)
            continue;
        const QModelIndex index = model.index(static_cast<int>(row), 0);
        Q_EMIT model.dataChanged(index, index,
                                 { TImbuedObjectsModel::Imbuement1RemainingTimeStringRole, TImbuedObjectsModel::Imbuement1RemainingTimeMsRole,
                                   TImbuedObjectsModel::Imbuement1VisualizationRole, TImbuedObjectsModel::Imbuement2RemainingTimeStringRole,
                                   TImbuedObjectsModel::Imbuement2RemainingTimeMsRole, TImbuedObjectsModel::Imbuement2VisualizationRole,
                                   TImbuedObjectsModel::Imbuement3RemainingTimeStringRole, TImbuedObjectsModel::Imbuement3RemainingTimeMsRole,
                                   TImbuedObjectsModel::Imbuement3VisualizationRole });
    }
}

TImbuementTrackerWidgetController::~TImbuementTrackerWidgetController() { m_model->setImbuedObjects({}); }

void TImbuementTrackerWidgetController::setGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    for (const QMetaObject::Connection& connection : m_sessionConnections)
        disconnect(connection);
    m_sessionConnections.clear();
    m_gameSession = GameSession;
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(GameSession.get());
    m_imbuementDurationsStorage = session ? session->imbuementDurationsStorage() : nullptr;
    if (m_imbuementDurationsStorage)
        m_sessionConnections.push_back(connect(m_imbuementDurationsStorage.get(),
                                               &cip::tibia::imbuing::TImbuementDurationsStorage::imbuedObjectsChanged, this,
                                               &TImbuementTrackerWidgetController::onImbuedObjectsChanged));
    // Host: qtclient keeps the widget across sessions, so it starts from the new session's objects.
    m_model->setImbuedObjects(m_imbuementDurationsStorage ? m_imbuementDurationsStorage->imbuedObjects()
                                                          : std::vector<imbuing::TImbuedObjectInformation>{});
    m_proxy->invalidate();
}

QObject* TImbuementTrackerWidgetController::imbuedObjectsModel() const { return m_proxy; }

QQuickItem* TImbuementTrackerWidgetController::gameWindowQuickItem() const { return widget()->gameWindowItem(); }

void TImbuementTrackerWidgetController::onCloseClicked() { onRequestClose(); }
void TImbuementTrackerWidgetController::onRequestClose() { Q_EMIT widgetClosed(); }
void TImbuementTrackerWidgetController::onRequestMoveToSidebar(const int TargetSidebar)
{
    widget()->requestMoveToSidebar(TargetSidebar);
}
void TImbuementTrackerWidgetController::onSetDragActive(const bool Active) { widget()->setDragActive(Active); }
void TImbuementTrackerWidgetController::onSidebarDestroyed() { widget()->onSidebarDestroyed(); }
void TImbuementTrackerWidgetController::onLevelUpClicked() {}
void TImbuementTrackerWidgetController::setCursorToResizeVertical(const bool Enabled)
{
    widget()->setCursorToResizeVertical(Enabled);
}
void TImbuementTrackerWidgetController::startContentSizeChangedTimer() { widget()->startContentSizeChangedTimer(); }
void TImbuementTrackerWidgetController::onWidgetQuickItemDestroyed() {}

void TImbuementTrackerWidgetController::applyFilterOptionsFromJson(const QJsonObject& json)
{
    m_showAlmostGone = json.value(QLatin1String("showAlmostGone")).toBool(true);
    m_showUsed = json.value(QLatin1String("showUsed")).toBool(true);
    m_showAlmostNew = json.value(QLatin1String("showAlmostNew")).toBool(true);
    m_showEmptySlots = json.value(QLatin1String("showEmptySlots")).toBool(true);
    m_proxy->setShowFlags(m_showAlmostGone, m_showUsed, m_showAlmostNew, m_showEmptySlots);
}

void TImbuementTrackerWidgetController::toggleShowFlag(bool& flag)
{
    flag = !flag;
    m_proxy->setShowFlags(m_showAlmostGone, m_showUsed, m_showAlmostNew, m_showEmptySlots);
    saveWidgetOptions();
}

void TImbuementTrackerWidgetController::setSidebarOptions(config::TSidebarOptions* options)
{
    m_options = options;
    if (!m_options)
        return;
    const QJsonObject json = m_options->section(QLatin1String(OptionsKey)).toObject();
    if (json.contains(QLatin1String("contentHeight")))
        widget()->setInitialContentHeightAndMaximized(json.value(QLatin1String("contentHeight")).toInt(),
                                                       json.value(QLatin1String("contentMaximized")).toBool(true));
    applyFilterOptionsFromJson(json);
}

void TImbuementTrackerWidgetController::onObjectClicked(const int SlotIDRaw, const int MouseButtonRaw,
                                                        const int KeyboardModifiersRaw)
{
    Q_UNUSED(SlotIDRaw);
    Q_UNUSED(MouseButtonRaw);
    Q_UNUSED(KeyboardModifiersRaw);
}

void TImbuementTrackerWidgetController::optionsContextMenu()
{
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    // The four filters; the labels carry the minutes thresholds (1 and 3).
    const auto addFlag = [this, menu](const QString& text, bool& flag) {
        TContextMenuEntryItem* entry = menu->addCustomEntry(text);
        entry->setCheckable(true);
        entry->setChecked(flag);
        connect(entry, &TContextMenuEntryItem::triggered, this, [this, &flag] { toggleShowFlag(flag); });
    };
    addFlag(qtTrId("imbuementtrackerwidget_show_almoste_gone").arg(1), m_showAlmostGone);
    addFlag(qtTrId("imbuementtrackerwidget_show_used").arg(1).arg(3), m_showUsed);
    addFlag(qtTrId("imbuementtrackerwidget_show_almost_new").arg(3), m_showAlmostNew);
    addFlag(qtTrId("imbuementtrackerwidget_show_without_imbuements"), m_showEmptySlots);
    menu->showCustomMenu();
}

void TImbuementTrackerWidgetController::onImbuedObjectsChanged()
{
    if (m_imbuementDurationsStorage)
        m_model->setImbuedObjects(m_imbuementDurationsStorage->imbuedObjects());
}

void TImbuementTrackerWidgetController::saveWidgetOptions()
{
    if (!m_options)
        return;
    QJsonObject json = m_options->section(QLatin1String(OptionsKey)).toObject();
    json.insert(QStringLiteral("contentHeight"), widget()->maximizedContentHeight());
    json.insert(QStringLiteral("contentMaximized"), widget()->isMaximized());
    json.insert(QStringLiteral("showAlmostGone"), m_showAlmostGone);
    json.insert(QStringLiteral("showUsed"), m_showUsed);
    json.insert(QStringLiteral("showAlmostNew"), m_showAlmostNew);
    json.insert(QStringLiteral("showEmptySlots"), m_showEmptySlots);
    m_options->setSection(QLatin1String(OptionsKey), json);
}

void TImbuementTrackerWidgetController::onWidgetOptionsChanged() { saveWidgetOptions(); }

TImbuementTrackerWidgetManager::TImbuementTrackerWidgetManager(QQuickItem* gameWindowItem, QQmlEngine* engine,
                                                               QObject* parent)
    : QObject(parent)
    , m_gameWindowItem(gameWindowItem)
    , m_engine(engine)
{}

TImbuementTrackerWidgetManager::~TImbuementTrackerWidgetManager() { delete m_controller; }

TSidebarWidget* TImbuementTrackerWidgetManager::open(config::TSidebarOptions* options)
{
    if (!m_controller) {
        m_controller = new TImbuementTrackerWidgetController(m_gameWindowItem, m_engine);
        m_controller->setSidebarOptions(options);
        m_controller->setGameSession(m_gameSession);
    }
    return m_controller->widget();
}

void TImbuementTrackerWidgetManager::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    m_gameSession = GameSession;
    if (m_controller)
        m_controller->setGameSession(GameSession);
}

void TImbuementTrackerWidgetManager::close(const bool saveWidgetOptions)
{
    if (!m_controller)
        return;
    if (saveWidgetOptions)
        m_controller->saveWidgetOptions();
    m_controller->deleteLater();
    m_controller = nullptr;
}

TSidebarWidget* TImbuementTrackerWidgetManager::current() const { return m_controller ? m_controller->widget() : nullptr; }

}
