#include "creaturetrackerwidgetcontroller.h"

#include "bossbestiary/bosstrackerstorage.h"
#include "config/sidebaroptions.h"
#include "gamewindow/contextmenucontroller.h"
#include "input/gameactionhandlers.h"
#include "qmlcomponents/texthelper.h"

#include <tibia/game/gamesession.h>

#include <QDateTime>
#include <QJsonObject>
#include <QHash>

#include <algorithm>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;
using Mode = TCreatureTrackerWidgetController::EWidgetMode;

constexpr auto BestiaryOptionsKey = "bestiaryTrackerWidgetOptions";
constexpr auto BosstiaryOptionsKey = "bosstiaryTrackerWidgetOptions";

const char* optionsKey(const Mode mode)
{
    return mode == Mode::Bosstiary ? BosstiaryOptionsKey : BestiaryOptionsKey;
}

} // namespace

TCreatureTrackerModel::TCreatureTrackerModel(QObject* parent)
    : cip::TCreatureTrackerModel(parent)
{}

int TCreatureTrackerModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant TCreatureTrackerModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_rows.size()))
        return {};
    const Row& row = m_rows[static_cast<size_t>(index.row())];
    const cyclopedia::TCreatureTrackerEntry& entry = row.entry;
    const qint64 nowSeconds = m_currentTime / 1000;
    switch (role) {
    case RaceIdRole:
        return static_cast<uint>(static_cast<quint16>(entry.raceId));
    case NameRole:
        return entry.name;
    case CurrentKillsRole:
        return entry.currentKills;
    case KillsForDetails1Role:
        return static_cast<uint>(static_cast<quint16>(entry.killsForDetails1));
    case KillsForDetails2Role:
        return static_cast<uint>(static_cast<quint16>(entry.killsForDetails2));
    case KillsForDetails3Role:
        return static_cast<uint>(static_cast<quint16>(entry.killsForDetails3));
    case CompletionPercentRole:
        return static_cast<float>(entry.currentKills) / std::max(1.0f, static_cast<float>(static_cast<quint16>(entry.killsForDetails3)));
    case RemainingKillsRole: {
        const int remaining = static_cast<int>(static_cast<quint16>(entry.killsForDetails3)) - static_cast<int>(entry.currentKills);
        return static_cast<uint>(std::max(0, remaining));
    }
    case IsFullyUnlockedRole:
        return entry.isFullyUnlocked();
    case CreatureHasCooldownRole:
        return row.hasCooldown;
    case CooldownIsExpiredRole:
        return static_cast<qint64>(row.cooldownEnd) <= nowSeconds;
    case CooldownEndRole:
        return row.cooldownEnd;
    case CooldownEndTextRole:
        if (!row.hasCooldown)
            return QString();
        if (nowSeconds < static_cast<qint64>(row.cooldownEnd))
        {
            static qmlcomponents::TTextHelperQmlService textHelper;
            return textHelper.formatSecondsToShortDaysHoursMinOrHoursMinOrMinSecondsOrSecondsTimeString(
                static_cast<int>(row.cooldownEnd - nowSeconds));
        }
        return qtTrId("bosstracker_no_cooldown");
    default:
        return {};
    }
}

QHash<int, QByteArray> TCreatureTrackerModel::roleNames() const
{
    return { { RaceIdRole, "raceID" },
             { NameRole, "name" },
             { CurrentKillsRole, "currentKills" },
             { KillsForDetails1Role, "killsForDetails1" },
             { KillsForDetails2Role, "killsForDetails2" },
             { KillsForDetails3Role, "killsForDetails3" },
             { CompletionPercentRole, "completionPercent" },
             { RemainingKillsRole, "remainingKills" },
             { IsFullyUnlockedRole, "isFullyUnlocked" },
             { CreatureHasCooldownRole, "creatureHasCooldown" },
             { CooldownIsExpiredRole, "cooldownIsExpired" },
             { CooldownEndRole, "cooldownEnd" },
             { CooldownEndTextRole, "cooldownEndText" } };
}

void TCreatureTrackerModel::setEntries(const std::vector<cyclopedia::TCreatureTrackerEntry>& entries)
{
    beginResetModel();
    m_rows.clear();
    m_rows.reserve(entries.size());
    for (const cyclopedia::TCreatureTrackerEntry& entry : entries)
        m_rows.push_back({ entry });
    endResetModel();
}

void TCreatureTrackerModel::notifyCooldownChanged(const int row)
{
    const QModelIndex changed = index(row, 0);
    Q_EMIT dataChanged(changed, changed, { CreatureHasCooldownRole, CooldownEndRole, CooldownEndTextRole, CooldownIsExpiredRole });
}

TCreatureTrackerSortProxyModel::TCreatureTrackerSortProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    setSourceModel(&m_source);
    sort(0);
}

void TCreatureTrackerSortProxyModel::setSortType(const int sortType)
{
    if (m_sortType == sortType)
        return;
    m_sortType = sortType;
    invalidate();
}

void TCreatureTrackerSortProxyModel::setSortOrder(const int sortOrder)
{
    if (m_sortOrder == sortOrder)
        return;
    m_sortOrder = sortOrder;
    invalidate();
}

/// By name, completion or remaining kills, ascending or descending; equal
/// values fall back to the name, ascending.
bool TCreatureTrackerSortProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    using Role = TCreatureTrackerModel::Role;
    const QString leftName = left.data(Role::NameRole).toString().toLower();
    const QString rightName = right.data(Role::NameRole).toString().toLower();
    const bool descending = m_sortOrder != Ascending;
    if (m_sortType == SortByPercentage) {
        const float a = left.data(Role::CompletionPercentRole).toFloat();
        const float b = right.data(Role::CompletionPercentRole).toFloat();
        if (a != b)
            return descending ? a > b : a < b;
        return leftName < rightName;
    }
    if (m_sortType == SortByRemaining) {
        const int a = left.data(Role::RemainingKillsRole).toInt();
        const int b = right.data(Role::RemainingKillsRole).toInt();
        if (a != b)
            return descending ? b < a : a < b;
        return leftName < rightName;
    }
    return descending ? rightName < leftName : leftName < rightName;
}

TCreatureTrackerWidgetController::TCreatureTrackerWidgetController(const EWidgetMode mode, cyclopedia::TCreatureTrackerStorage* creatures,
                                                                   bossbestiary::TBossTrackerStorage* bosses, QQuickItem* gameWindowItem,
                                                                   QQmlEngine* engine, QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
    , m_mode(mode)
    , m_creatures(creatures)
    , m_bosses(bosses)
    , m_model(new TCreatureTrackerSortProxyModel(this))
{
    m_widgetMode = mode;
    m_trackedCreatures = m_model;
    if (m_creatures) {
        if (mode == Mode::Bosstiary)
            connect(m_creatures, &cyclopedia::TCreatureTrackerStorage::trackedBossesChanged, this,
                    &TCreatureTrackerWidgetController::onTrackedCreaturesChanged);
        else
            connect(m_creatures, &cyclopedia::TCreatureTrackerStorage::trackedMonstersChanged, this,
                    &TCreatureTrackerWidgetController::onTrackedCreaturesChanged);
    }
    if (m_bosses && mode == Mode::Bosstiary)
        connect(m_bosses, &bossbestiary::TBossTrackerStorage::bossEntriesChanged, this,
                &TCreatureTrackerWidgetController::onBossCooldownsChanged);
    onTrackedCreaturesChanged();
    m_cooldownTimer.setInterval(1000);
    connect(&m_cooldownTimer, &QTimer::timeout, this, &TCreatureTrackerWidgetController::updateCooldownExpiry);
    m_cooldownTimer.start();
    widget()->createItem(QStringLiteral("CreatureTrackerWidget.qml"));
    if (QQuickItem* item = quickItem())
        connect(item, &QObject::destroyed, this, &TCreatureTrackerWidgetController::onWidgetQuickItemDestroyed);
}

QObject* TCreatureTrackerWidgetController::trackedCreatures() const { return m_model; }

TCreatureTrackerWidgetController::EWidgetMode TCreatureTrackerWidgetController::widgetMode() const { return m_mode; }

QQuickItem* TCreatureTrackerWidgetController::gameWindowQuickItem() const { return widget()->gameWindowItem(); }

void TCreatureTrackerWidgetController::onCloseClicked() { onRequestClose(); }
void TCreatureTrackerWidgetController::onRequestClose() { Q_EMIT widgetClosed(); }
void TCreatureTrackerWidgetController::onRequestMoveToSidebar(const int TargetSidebar)
{
    widget()->requestMoveToSidebar(TargetSidebar);
}
void TCreatureTrackerWidgetController::onSetDragActive(const bool Active) { widget()->setDragActive(Active); }
void TCreatureTrackerWidgetController::onSidebarDestroyed() { widget()->onSidebarDestroyed(); }
void TCreatureTrackerWidgetController::onLevelUpClicked() {}
void TCreatureTrackerWidgetController::setCursorToResizeVertical(const bool Enabled)
{
    widget()->setCursorToResizeVertical(Enabled);
}
void TCreatureTrackerWidgetController::startContentSizeChangedTimer() { widget()->startContentSizeChangedTimer(); }
void TCreatureTrackerWidgetController::onWidgetQuickItemDestroyed() {}

void TCreatureTrackerWidgetController::setSidebarOptions(config::TSidebarOptions* options)
{
    m_options = options;
    if (!m_options)
        return;
    const QJsonObject json = m_options->section(QLatin1String(optionsKey(m_mode))).toObject();
    if (json.contains(QLatin1String("contentHeight")))
        widget()->setInitialContentHeightAndMaximized(json.value(QLatin1String("contentHeight")).toInt(),
                                                       json.value(QLatin1String("contentMaximized")).toBool(true));
}

void TCreatureTrackerWidgetController::saveWidgetOptions() { persistOptions(); }

void TCreatureTrackerWidgetController::onWidgetOptionsChanged() { persistOptions(); }

void TCreatureTrackerWidgetController::persistOptions()
{
    if (!m_options)
        return;
    QJsonObject json = m_options->section(QLatin1String(optionsKey(m_mode))).toObject();
    json.insert(QStringLiteral("contentHeight"), widget()->maximizedContentHeight());
    json.insert(QStringLiteral("contentMaximized"), widget()->isMaximized());
    m_options->setSection(QLatin1String(optionsKey(m_mode)), json);
}

/// Takes the storage's list for the widget's mode, then the cooldowns.
void TCreatureTrackerWidgetController::onTrackedCreaturesChanged()
{
    if (!m_creatures)
        return;
    if (m_mode == Mode::Bestiary)
        m_model->source()->setEntries(m_creatures->monsters());
    if (m_mode == Mode::Bosstiary)
        m_model->source()->setEntries(m_creatures->bosses());
    onBossCooldownsChanged();
    updateCooldownExpiry();
}

/// Bosstiary: copies each tracked boss's cooldown from the boss tracker and
/// refreshes the rows whose cooldown changed.
void TCreatureTrackerWidgetController::onBossCooldownsChanged()
{
    if (!m_creatures)
        return;
    if (m_mode == Mode::Bosstiary && m_bosses) {
        TCreatureTrackerModel* model = m_model->source();
        std::vector<TCreatureTrackerModel::Row>& rows = model->rows();
        for (size_t i = 0; i < rows.size(); ++i) {
            TCreatureTrackerModel::Row& row = rows[i];
            const qint64 cooldown = m_bosses->cooldownUntil(row.entry.raceId);
            const bool found = cooldown != 0;
            const uint previous = row.cooldownEnd;
            const bool hadCooldown = row.hasCooldown;
            uint current = 0;
            if (found) {
                current = static_cast<uint>(cooldown);
                row.cooldownEnd = current;
                row.hasCooldown = true;
            }
            if (found != hadCooldown || previous != current)
                model->notifyCooldownChanged(static_cast<int>(i));
        }
    }
    updateCooldownExpiry();
}

/// Once a second of game time: refreshes the rows whose cooldown ended in
/// that time, or still runs.
void TCreatureTrackerWidgetController::updateCooldownExpiry()
{
    if (!m_creatures || !m_model)
        return;
    TCreatureTrackerModel* model = m_model->source();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 last = model->currentTime();
    if (last / 1000 >= now / 1000)
        return;
    model->setCurrentTime(now);
    std::vector<TCreatureTrackerModel::Row>& rows = model->rows();
    for (size_t i = 0; i < rows.size(); ++i) {
        TCreatureTrackerModel::Row& row = rows[i];
        const qint64 end = static_cast<qint64>(row.cooldownEnd) * 1000;
        if (end <= now && end > last) {
            row.cooldownEnd = 0;
            row.hasCooldown = true;
            model->notifyCooldownChanged(static_cast<int>(i));
        } else if (end > now || end > last) {
            model->notifyCooldownChanged(static_cast<int>(i));
        }
    }
}

void TCreatureTrackerWidgetController::requestOpenBestiary()
{
    auto action = std::make_shared<input::TGameActionOpenCyclopedia>(m_mode == Mode::Bosstiary ? 6 : 1);
    input::publish(action);
}

void TCreatureTrackerWidgetController::requestOpenBestiaryEntry(const uint RaceID)
{
    auto action = std::make_shared<input::TGameActionOpenCyclopedia>(m_mode == Mode::Bosstiary ? 6 : 1);
    action->setRaceId(static_cast<uint16_t>(static_cast<int>(RaceID)));
    input::publish(action);
}

void TCreatureTrackerWidgetController::requestContextMenu(const uint RaceID, const QString& RaceName)
{
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    connect(menu->addCustomEntry(qtTrId("bestiary_tracker_stop_tracking").arg(RaceName)), &TContextMenuEntryItem::triggered,
            this, [RaceID] { tibia::game::sendStatusTrackerBestiary(static_cast<uint16_t>(RaceID), false); });
    menu->showCustomMenu();
}

/// Sort by name, completion or remaining kills; ascending or descending.
void TCreatureTrackerWidgetController::requestWidgetConfigurationMenu()
{
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    using Proxy = TCreatureTrackerSortProxyModel;
    TCreatureTrackerSortProxyModel* proxy = m_model;
    menu->beginCustomMenu();
    const auto addChoice = [&](const char* text, const bool checked, auto apply) {
        TContextMenuEntryItem* entry = menu->addCustomEntry(qtTrId(text));
        entry->setCheckable(true);
        entry->setChecked(checked);
        connect(entry, &TContextMenuEntryItem::triggered, this, [this, proxy, apply] {
            apply(proxy);
            saveWidgetOptions();
        });
    };
    addChoice("bestiary_tracker_sort_name", proxy->sortType() == Proxy::SortByName,
              [](Proxy* p) { p->setSortType(Proxy::SortByName); });
    addChoice("bestiary_tracker_sort_percentage", proxy->sortType() == Proxy::SortByPercentage,
              [](Proxy* p) { p->setSortType(Proxy::SortByPercentage); });
    addChoice("bestiary_tracker_sort_remaining", proxy->sortType() == Proxy::SortByRemaining,
              [](Proxy* p) { p->setSortType(Proxy::SortByRemaining); });
    menu->addCustomSeparator();
    addChoice("bestiary_tracker_sort_ascending", proxy->sortOrder() == Proxy::Ascending,
              [](Proxy* p) { p->setSortOrder(Proxy::Ascending); });
    addChoice("bestiary_tracker_sort_descending", proxy->sortOrder() == Proxy::Descending,
              [](Proxy* p) { p->setSortOrder(Proxy::Descending); });
    menu->showCustomMenu();
}

TCreatureTrackerWidgetManager::TCreatureTrackerWidgetManager(const TCreatureTrackerWidgetController::EWidgetMode mode,
                                                             cyclopedia::TCreatureTrackerStorage* creatures,
                                                             bossbestiary::TBossTrackerStorage* bosses, QQuickItem* gameWindowItem,
                                                             QQmlEngine* engine, QObject* parent)
    : QObject(parent)
    , m_mode(mode)
    , m_creatures(creatures)
    , m_bosses(bosses)
    , m_gameWindowItem(gameWindowItem)
    , m_engine(engine)
{}

TCreatureTrackerWidgetManager::~TCreatureTrackerWidgetManager() { delete m_controller; }

TSidebarWidget* TCreatureTrackerWidgetManager::open(config::TSidebarOptions* options)
{
    if (!m_controller) {
        m_controller = new TCreatureTrackerWidgetController(m_mode, m_creatures, m_bosses, m_gameWindowItem, m_engine);
        m_controller->setSidebarOptions(options);
    }
    return m_controller->widget();
}

void TCreatureTrackerWidgetManager::close(const bool saveWidgetOptions)
{
    if (!m_controller)
        return;
    if (saveWidgetOptions)
        m_controller->saveWidgetOptions();
    m_controller->deleteLater();
    m_controller = nullptr;
}

TSidebarWidget* TCreatureTrackerWidgetManager::current() const { return m_controller ? m_controller->widget() : nullptr; }

} // namespace tibia::gamewindow
