#include "bosstrackerwidgetcontroller.h"

#include "config/sidebaroptions.h"
#include "gamewindow/contextmenucontroller.h"
#include "input/gameactionhandlers.h"
#include "qmlcomponents/outfittypes.h"
#include "qmlcomponents/qmlcomponentsciptypes.h"

#include <tibia/assets/things.h>

#include <QDateTime>

#include <algorithm>
#include <QJsonObject>
#include <tibia/game/thingsession_forward.h>

#include "qmlcomponents/texthelper.h"

#include <QQuickWindow>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;
constexpr auto OptionsKey = "bossCooldownsWidgetOptions";

qmlcomponents::TOutfitQmlModel* outfitForRace(const int raceId, QObject* parent)
{
    const RaceType& race = tibia::game::raceData(static_cast<uint32_t>(raceId));
    const Outfit& outfit = race.outfit;
    auto* model = new qmlcomponents::TOutfitQmlModel(parent);
    const bool objectOutfit = outfit.getId() == 0 && outfit.getAuxId() != 0;
    const uint outfitId = objectOutfit ? outfit.getAuxId() : outfit.getId();
    model->setIsObjectOutfit(objectOutfit);
    model->setOutfitID(outfitId);
    model->setFirstAddOn((outfit.getAddons() & 1) != 0);
    model->setSecondAddOn((outfit.getAddons() & 2) != 0);
    auto* colors = new qmlcomponents::TOutfitColorQmlType(outfit.getHead(), outfit.getBody(), outfit.getLegs(), outfit.getFeet(), model);
    model->setColors(colors);
    return model;
}

} // namespace

TBossTrackerListModel::TBossTrackerListModel(QObject* parent)
    : cip::TBossTrackerModel(parent)
{}

TBossTrackerListModel::~TBossTrackerListModel() { clearOutfits(); }

int TBossTrackerListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_visible.size());
}

QVariant TBossTrackerListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_visible.size()))
        return {};
    const Row& row = m_all[static_cast<size_t>(m_visible[static_cast<size_t>(index.row())])];
    // CIP: a cooldown runs while its end lies ahead; the text is the time left.
    const qint64 nowSeconds = m_currentTimeMs / 1000;
    switch (role) {
    case NameRole:
        return row.entry.name;
    case BossRaceIdRole:
        return row.entry.raceId;
    case OutfitRole:
        return QVariant::fromValue<QObject*>(row.outfit);
    case HasCooldownRole:
        return nowSeconds < row.entry.cooldownUntil;
    case CooldownEndStringRole:
        return qmlcomponents::shortDurationString(row.entry.cooldownUntil - nowSeconds);
    case CooldownEndRole:
        return static_cast<uint>(row.entry.cooldownUntil);
    default:
        return {};
    }
}

QHash<int, QByteArray> TBossTrackerListModel::roleNames() const
{
    return { { NameRole, "name" },
             { BossRaceIdRole, "bossRaceId" },
             { OutfitRole, "outfit" },
             { HasCooldownRole, "hasCooldown" },
             { CooldownEndStringRole, "cooldownEndString" },
             { CooldownEndRole, "cooldownEnd" } };
}

void TBossTrackerListModel::clearOutfits()
{
    for (Row& row : m_all) {
        if (row.outfit) {
            row.outfit->deleteLater();
            row.outfit = nullptr;
        }
    }
}

void TBossTrackerListModel::setRows(std::vector<Row> rows)
{
    beginResetModel();
    clearOutfits();
    m_all = std::move(rows);
    rebuildVisible();
    endResetModel();
}

void TBossTrackerListModel::setCurrentTime(const qint64 currentTimeMs)
{
    const qint64 previousMs = std::min(m_currentTimeMs, currentTimeMs);
    if (m_currentTimeMs / 1000 >= currentTimeMs / 1000)
        return;
    m_currentTimeMs = currentTimeMs;
    for (int row = 0; row < static_cast<int>(m_visible.size()); ++row) {
        const qint64 endMs = m_all[static_cast<size_t>(m_visible[static_cast<size_t>(row)])].entry.cooldownUntil * 1000;
        if (endMs != previousMs && previousMs <= endMs)
            Q_EMIT dataChanged(index(row), index(row), { CooldownEndStringRole, HasCooldownRole });
    }
}

void TBossTrackerListModel::setNameFilter(const QString& filter)
{
    if (m_filter == filter)
        return;
    beginResetModel();
    m_filter = filter;
    rebuildVisible();
    endResetModel();
}

void TBossTrackerListModel::setSortMode(const int mode)
{
    if (m_sortMode == mode)
        return;
    beginResetModel();
    m_sortMode = mode;
    rebuildVisible();
    endResetModel();
}

void TBossTrackerListModel::rebuildVisible()
{
    m_visible.clear();
    m_visible.reserve(m_all.size());
    for (int i = 0; i < static_cast<int>(m_all.size()); ++i) {
        if (m_filter.isEmpty() || m_all[static_cast<size_t>(i)].entry.name.contains(m_filter, Qt::CaseInsensitive))
            m_visible.push_back(i);
    }
    std::stable_sort(m_visible.begin(), m_visible.end(), [this](const int left, const int right) {
        return lessThan(m_all[static_cast<size_t>(left)], m_all[static_cast<size_t>(right)]);
    });
}

bool TBossTrackerListModel::lessThan(const Row& left, const Row& right) const
{
    const QString leftName = left.entry.name.toLower();
    const QString rightName = right.entry.name.toLower();
    if (m_sortMode == SortByRemainingCooldown) {
        const bool leftHas = left.entry.hasCooldown();
        const bool rightHas = right.entry.hasCooldown();
        if (leftHas != rightHas)
            return leftHas;
        const qint64 leftEnd = leftHas ? left.entry.cooldownUntil : 0;
        const qint64 rightEnd = rightHas ? right.entry.cooldownUntil : 0;
        if (leftEnd != rightEnd)
            return leftEnd < rightEnd;
    }
    return QString::compare(leftName, rightName, Qt::CaseSensitive) < 0;
}

TBossTrackerWidgetController::TBossTrackerWidgetController(bossbestiary::TBossTrackerStorage* storage, QQuickItem* gameWindowItem,
                                                           QQmlEngine* engine, QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
    , m_storage(storage)
    , m_model(new TBossTrackerListModel(this))
{
    m_bosses = m_model;
    if (m_storage) {
        connect(m_storage, &bossbestiary::TBossTrackerStorage::bossEntriesChanged, this,
                &TBossTrackerWidgetController::onBossTrackingEntriesChanged);
    }
    m_model->setCurrentTime(QDateTime::currentMSecsSinceEpoch());
    onBossTrackingEntriesChanged();
    widget()->createItem(QStringLiteral("BossTrackingWidget.qml"));
    // CIP: the sidebar widgets' frame update gives the model the time.
    const auto followFrames = [this](QQuickWindow* window) {
        if (window)
            connect(window, &QQuickWindow::afterAnimating, this, [this] { m_model->setCurrentTime(QDateTime::currentMSecsSinceEpoch()); });
    };
    if (gameWindowItem) {
        followFrames(gameWindowItem->window());
        connect(gameWindowItem, &QQuickItem::windowChanged, this, followFrames);
    }
    if (QQuickItem* item = quickItem()) {
        m_focus.setClickArea(item);
        connect(item, &QObject::destroyed, this, &TBossTrackerWidgetController::onWidgetQuickItemDestroyed);
    }
    connect(&m_focus, &TUiFocusManager::focusChanged, this, &TBossTrackerWidgetController::focusChanged);
}

QObject* TBossTrackerWidgetController::bosses() const { return m_model; }

bool TBossTrackerWidgetController::hasUnlockedAnyBosses() const { return m_storage && m_storage->hasUnlockedAnyBosses(); }

QQuickItem* TBossTrackerWidgetController::gameWindowQuickItem() const { return widget()->gameWindowItem(); }
bool TBossTrackerWidgetController::hasFocus() const { return m_focus.hasFocus(); }

void TBossTrackerWidgetController::onCloseClicked() { onRequestClose(); }
void TBossTrackerWidgetController::onRequestClose() { Q_EMIT widgetClosed(); }
void TBossTrackerWidgetController::onRequestMoveToSidebar(const int TargetSidebar)
{
    widget()->requestMoveToSidebar(TargetSidebar);
}
void TBossTrackerWidgetController::onSetDragActive(const bool Active) { widget()->setDragActive(Active); }
void TBossTrackerWidgetController::onSidebarDestroyed() { widget()->onSidebarDestroyed(); }
void TBossTrackerWidgetController::onLevelUpClicked() {}
void TBossTrackerWidgetController::setCursorToResizeVertical(const bool Enabled)
{
    widget()->setCursorToResizeVertical(Enabled);
}
void TBossTrackerWidgetController::startContentSizeChangedTimer() { widget()->startContentSizeChangedTimer(); }
void TBossTrackerWidgetController::onWidgetQuickItemDestroyed() {}

// CIP TSidebarWidgetControllerWithFocusHandling: the widget's focus is its
// TUiFocusManager's (a click outside the widget gives it back).
void TBossTrackerWidgetController::takeFocus()
{
    m_focus.takeFocus();
}

void TBossTrackerWidgetController::releaseFocus()
{
    m_focus.releaseFocus();
}

void TBossTrackerWidgetController::setSidebarOptions(config::TSidebarOptions* options)
{
    m_options = options;
    if (!m_options)
        return;
    const QJsonObject json = m_options->section(QLatin1String(OptionsKey)).toObject();
    m_model->setSortMode(json.value(QLatin1String("sortType")).toInt(TBossTrackerListModel::SortByRemainingCooldown));
    if (json.contains(QLatin1String("contentHeight")))
        widget()->setInitialContentHeightAndMaximized(json.value(QLatin1String("contentHeight")).toInt(),
                                                       json.value(QLatin1String("contentMaximized")).toBool(true));
}

void TBossTrackerWidgetController::saveWidgetOptions() { persistOptions(); }

void TBossTrackerWidgetController::onWidgetOptionsChanged() { persistOptions(); }

void TBossTrackerWidgetController::persistOptions()
{
    if (!m_options)
        return;
    QJsonObject json = m_options->section(QLatin1String(OptionsKey)).toObject();
    json.insert(QStringLiteral("contentHeight"), widget()->maximizedContentHeight());
    json.insert(QStringLiteral("contentMaximized"), widget()->isMaximized());
    json.insert(QStringLiteral("sortType"), m_model->sortMode());
    m_options->setSection(QLatin1String(OptionsKey), json);
}

void TBossTrackerWidgetController::onBossTrackingEntriesChanged()
{
    if (!m_storage)
        return;
    std::vector<TBossTrackerListModel::Row> rows;
    rows.reserve(m_storage->entries().size());
    for (const bossbestiary::TBossTrackerEntry& entry : m_storage->entries()) {
        TBossTrackerListModel::Row row;
        row.entry = entry;
        row.outfit = outfitForRace(entry.raceId, m_model);
        rows.push_back(std::move(row));
    }
    m_model->setRows(std::move(rows));
    Q_EMIT bossCountChanged();
}

void TBossTrackerWidgetController::requestFilterByBossName(const QString& Filter) { m_model->setNameFilter(Filter); }

void TBossTrackerWidgetController::requestOpenBosstiary(const int BossRaceID)
{
    auto action = std::make_shared<input::TGameActionOpenCyclopedia>(6);
    action->setRaceId(static_cast<uint16_t>(BossRaceID));
    Q_EMIT publishGameAction(action);
}

/// Two checkable sort entries; picking one sorts and saves the options.
void TBossTrackerWidgetController::requestWidgetConfigurationMenu()
{
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    static constexpr struct
    {
        const char* text;
        int mode;
    } Entries[] = { { "bosstracker_sort_remaining_cooldown", TBossTrackerListModel::SortByRemainingCooldown },
                    { "bosstracker_sort_name", TBossTrackerListModel::SortByName } };
    for (const auto& [text, mode] : Entries) {
        TContextMenuEntryItem* entry = menu->addCustomEntry(qtTrId(text));
        entry->setCheckable(true);
        entry->setChecked(m_model->sortMode() == mode);
        connect(entry, &TContextMenuEntryItem::triggered, this, [this, mode] {
            m_model->setSortMode(mode);
            saveWidgetOptions();
        });
    }
    menu->showCustomMenu();
}

TBossTrackerWidgetManager::TBossTrackerWidgetManager(bossbestiary::TBossTrackerStorage* storage, QQuickItem* gameWindowItem, QQmlEngine* engine,
                                                     QObject* parent)
    : QObject(parent)
    , m_storage(storage)
    , m_gameWindowItem(gameWindowItem)
    , m_engine(engine)
{}

TBossTrackerWidgetManager::~TBossTrackerWidgetManager() { delete m_controller; }

TSidebarWidget* TBossTrackerWidgetManager::open(config::TSidebarOptions* options)
{
    if (!m_controller) {
        m_controller = new TBossTrackerWidgetController(m_storage, m_gameWindowItem, m_engine);
        connect(m_controller, &TBossTrackerWidgetController::publishGameAction, this, &TBossTrackerWidgetManager::publishGameAction);
        m_controller->setSidebarOptions(options);
    }
    return m_controller->widget();
}

void TBossTrackerWidgetManager::close(const bool saveWidgetOptions)
{
    if (!m_controller)
        return;
    if (saveWidgetOptions)
        m_controller->saveWidgetOptions();
    m_controller->deleteLater();
    m_controller = nullptr;
}

TSidebarWidget* TBossTrackerWidgetManager::current() const { return m_controller ? m_controller->widget() : nullptr; }

} // namespace tibia::gamewindow
