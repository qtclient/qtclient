#include "imbuementtrackerwidgetcontroller.h"

#include "config/sidebaroptions.h"
#include "gamewindow/contextmenucontroller.h"

#include <client/item.h>
#include <client/staticdata.h>

#include <tibia/game/gamenotify.h>
#include <tibia/game/tgamenotify.h>

namespace tibia::gamewindow {

namespace {

using Args = std::vector<std::any>;
using Viz = cip::tibia::qmlcomponents::enums::EVisualizationType;
constexpr auto OptionsKey = "imbuementTrackerWidgetOptions";

QString formatDuration(const uint32_t seconds)
{
    const uint32_t h = seconds / 3600;
    const uint32_t m = (seconds % 3600) / 60;
    const uint32_t s = seconds % 60;
    return QStringLiteral("%1:%2:%3")
        .arg(h, 2, 10, QLatin1Char('0'))
        .arg(m, 2, 10, QLatin1Char('0'))
        .arg(s, 2, 10, QLatin1Char('0'));
}

int visualizationFor(const uint32_t duration, const bool running)
{
    if (!running)
        return static_cast<int>(Viz::Used);
    if (duration < 3600)
        return static_cast<int>(Viz::AlmostGone);
    if (duration < 12 * 3600)
        return static_cast<int>(Viz::Used);
    return static_cast<int>(Viz::AlmostNew);
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

QVariant TImbuedObjectsModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= int(m_rows.size()))
        return {};
    const Row& row = m_rows[size_t(index.row())];
    switch (role) {
    case InventoryPositionRole:
        return row.inventoryPosition;
    case ManagedContainerTooltipRole:
        return QString{};
    case TypeIdRole:
        return row.typeId;
    case UpgradeTierRole:
        return row.upgradeTier;
    case CumulativeCountRole:
        return row.cumulativeCount;
    case LiquidTypeRole:
        return row.liquidType;
    case HookDirectionRole:
        return row.hookDirection;
    case AlternativeSlotStringRole:
        return QString{};
    case SlotCountRole:
        return row.slotCount;
    case Imbuement1IconIdRole:
        return row.iconId[0];
    case Imbuement1NameRole:
        return row.name[0];
    case Imbuement1RemainingTimeStringRole:
        return row.remaining[0];
    case Imbuement1VisualizationRole:
        return row.visualization[0];
    case Imbuement2IconIdRole:
        return row.iconId[1];
    case Imbuement2NameRole:
        return row.name[1];
    case Imbuement2RemainingTimeStringRole:
        return row.remaining[1];
    case Imbuement2VisualizationRole:
        return row.visualization[1];
    case Imbuement3IconIdRole:
        return row.iconId[2];
    case Imbuement3NameRole:
        return row.name[2];
    case Imbuement3RemainingTimeStringRole:
        return row.remaining[2];
    case Imbuement3VisualizationRole:
        return row.visualization[2];
    default:
        return {};
    }
}

QHash<int, QByteArray> TImbuedObjectsModel::roleNames() const
{
    return {
        { InventoryPositionRole, "inventoryPosition" },
        { ManagedContainerTooltipRole, "managedContainerTooltip" },
        { TypeIdRole, "typeID" },
        { UpgradeTierRole, "upgradeTier" },
        { CumulativeCountRole, "cumulativeCount" },
        { LiquidTypeRole, "liquideType" },
        { HookDirectionRole, "hookDirection" },
        { AlternativeSlotStringRole, "alternativeSlotString" },
        { SlotCountRole, "slotCount" },
        { Imbuement1IconIdRole, "imbuement1IconID" },
        { Imbuement1NameRole, "imbuement1Name" },
        { Imbuement1RemainingTimeStringRole, "imbuement1RemainingTimeString" },
        { Imbuement1VisualizationRole, "imbuement1Visualization" },
        { Imbuement2IconIdRole, "imbuement2IconID" },
        { Imbuement2NameRole, "imbuement2Name" },
        { Imbuement2RemainingTimeStringRole, "imbuement2RemainingTimeString" },
        { Imbuement2VisualizationRole, "imbuement2Visualization" },
        { Imbuement3IconIdRole, "imbuement3IconID" },
        { Imbuement3NameRole, "imbuement3Name" },
        { Imbuement3RemainingTimeStringRole, "imbuement3RemainingTimeString" },
        { Imbuement3VisualizationRole, "imbuement3Visualization" },
    };
}

void TImbuedObjectsModel::setItems(std::vector<ImbuementTrackerItem> items)
{
    beginResetModel();
    m_rows.clear();
    m_rows.reserve(items.size());
    for (const ImbuementTrackerItem& item : items) {
        Row row;
        row.inventoryPosition = item.slot;
        row.slotCount = item.totalSlots;
        row.typeId = item.typeId;
        row.upgradeTier = item.tier;
        row.cumulativeCount = item.count;
        row.liquidType = item.fluidOrSubtype;
        for (const auto& [slotIndex, slot] : item.slots) {
            if (slotIndex >= 3)
                continue;
            row.iconId[slotIndex] = slot.iconId;
            row.name[slotIndex] = QString::fromStdString(slot.name);
            row.remaining[slotIndex] = formatDuration(slot.duration);
            row.visualization[slotIndex] = visualizationFor(slot.duration, slot.state);
            ++row.filledSlots;
        }
        m_rows.push_back(std::move(row));
    }
    endResetModel();
}

TImbuedObjectsSortFilterProxyModel::TImbuedObjectsSortFilterProxyModel(TImbuedObjectsModel* source, QObject* parent)
    : cip::TImbuedObjectsSortFilterProxyModel(parent)
    , m_source(source)
{
    setSourceModel(m_source);
    setDynamicSortFilter(true);
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
    if (!m_source || sourceRow < 0 || sourceRow >= m_source->rowCount())
        return false;
    const TImbuedObjectsModel::Row& row = m_source->m_rows[size_t(sourceRow)];
    if (row.filledSlots == 0)
        return m_showEmptySlots;
    for (int i = 0; i < 3; ++i) {
        if (row.name[i].isEmpty() && row.iconId[i] == 0)
            continue;
        if (showFlagEnabled(row.visualization[i], m_showAlmostGone, m_showUsed, m_showAlmostNew))
            return true;
    }
    return false;
}

TImbuementTrackerWidgetController::TImbuementTrackerWidgetController(QQuickItem* gameWindowItem, QQmlEngine* engine,
                                                                     QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
    , m_model(new TImbuedObjectsModel(this))
    , m_proxy(new TImbuedObjectsSortFilterProxyModel(m_model, this))
{
    m_imbuedObjectsModel = m_proxy;
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onUpdateImbuementTracker", [this](GameObject*, const Args& args) {
        if (const auto* list = args.empty() ? nullptr : std::any_cast<std::vector<ImbuementTrackerItem>>(&args[0])) {
            m_model->setItems(*list);
            onImbuedObjectsChanged();
        }
        return std::any{};
    });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        m_model->setItems({});
        onImbuedObjectsChanged();
    });
    widget()->createItem(QStringLiteral("ImbuementTrackerWidget.qml"));
    if (QQuickItem* item = quickItem())
        connect(item, &QObject::destroyed, this, &TImbuementTrackerWidgetController::onWidgetQuickItemDestroyed);
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
    menu->addCustomEntry(qtTrId("imbuement_tracker_widget_caption"));
    menu->addCustomSeparator();

    const auto addFlag = [this, menu](const char* trId, bool& flag) {
        TContextMenuEntryItem* entry = menu->addCustomEntry(qtTrId(trId));
        entry->setCheckable(true);
        entry->setChecked(flag);
        connect(entry, &TContextMenuEntryItem::triggered, this, [this, &flag] { toggleShowFlag(flag); });
    };
    addFlag("imbuementtrackerwidget_show_almoste_gone", m_showAlmostGone);
    addFlag("imbuementtrackerwidget_show_used", m_showUsed);
    addFlag("imbuementtrackerwidget_show_almost_new", m_showAlmostNew);
    addFlag("imbuementtrackerwidget_show_without_imbuements", m_showEmptySlots);
    menu->showCustomMenu();
}

void TImbuementTrackerWidgetController::onImbuedObjectsChanged()
{
    m_proxy->onOptionsChanged();
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
    }
    return m_controller->widget();
}

void TImbuementTrackerWidgetManager::close()
{
    if (!m_controller)
        return;
    m_controller->saveWidgetOptions();
    m_controller->deleteLater();
    m_controller = nullptr;
}

TSidebarWidget* TImbuementTrackerWidgetManager::current() const { return m_controller ? m_controller->widget() : nullptr; }

}
