#include "imbuementtrackerwidgetcontroller.h"

#include "config/sidebaroptions.h"
#include "gamewindow/contextmenucontroller.h"
#include "qmlcomponents/texthelper.h"

#include <client/staticdata.h>

#include <tibia/game/gamenotify.h>
#include <tibia/game/tgamenotify.h>

namespace tibia::gamewindow {

namespace {

using Args = std::vector<std::any>;
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

QVariant TImbuedObjectsModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= int(m_rows.size()))
        return {};
    const Row& row = m_rows[size_t(index.row())];
    if (role >= Imbuement1NameRole && role <= Imbuement3VisualizationRole) {
        const int slot = (role - Imbuement1NameRole) / 5;
        switch ((role - Imbuement1NameRole) % 5) {
        case 0:
            return row.name[slot];
        case 1:
            return row.iconId[slot];
        case 2:
            return remainingTimeString(row.remainingMs[slot]);
        case 3:
            return row.remainingMs[slot];
        default:
            return QVariant::fromValue(visualization(row.remainingMs[slot]));
        }
    }
    switch (role) {
    case InventoryPositionRole:
        return row.inventoryPosition;
    case SlotCountRole:
        return row.slotCount;
    case TypeIdRole:
        return row.typeId;
    case CumulativeCountRole:
        return row.cumulativeCount;
    case LiquidTypeRole:
        return row.liquidType;
    case HookDirectionRole:
        return row.hookDirection;
    case UpgradeTierRole:
        return row.upgradeTier;
    case TypeCountRole:
    case CountStringRole:
    case AlternativeSlotStringRole:
    case LootValueImageSourceUnderItemRole:
    case LootValueImageSourceOverItemRole:
    case ManagedContainerTooltipRole:
        return QString{};
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
            row.remainingMs[slotIndex] = qint64(slot.duration) * 1000;
            ++row.filledSlots;
        }
        m_rows.push_back(std::move(row));
    }
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
    if (row.filledSlots == 0)
        return m_showEmptySlots;
    for (int i = 0; i < 3; ++i) {
        if (row.name[i].isEmpty() && row.iconId[i] == 0)
            continue;
        if (showFlagEnabled(static_cast<int>(visualization(row.remainingMs[i])), m_showAlmostGone, m_showUsed, m_showAlmostNew))
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
