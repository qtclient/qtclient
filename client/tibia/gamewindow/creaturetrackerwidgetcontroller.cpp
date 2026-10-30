#include "creaturetrackerwidgetcontroller.h"

#include "bossbestiary/bosstrackerstorage.h"
#include "config/sidebaroptions.h"
#include "gamewindow/contextmenucontroller.h"
#include "input/gameactionhandlers.h"

#include <tibia/world/game.h>

#include <QDateTime>
#include <QJsonObject>

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

QString formatCooldownEnd(const qint64 until)
{
    if (until <= 0)
        return {};
    return QDateTime::fromSecsSinceEpoch(until).toString(QStringLiteral("yyyy-MM-dd, HH:mm:ss"));
}

} // namespace

TCreatureTrackerListModel::TCreatureTrackerListModel(QObject* parent)
    : cip::TCreatureTrackerModel(parent)
{}

int TCreatureTrackerListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant TCreatureTrackerListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_rows.size()))
        return {};
    const Row& row = m_rows[static_cast<size_t>(index.row())];
    switch (role) {
    case RaceIdRole:
        return row.entry.raceId;
    case NameRole:
        return row.entry.name;
    case CreatureHasCooldownRole:
        return row.creatureHasCooldown;
    case CooldownIsExpiredRole:
        return row.cooldownIsExpired;
    case CooldownEndTextRole:
        return row.cooldownEndText;
    case CurrentKillsRole:
        return static_cast<int>(row.entry.currentKills);
    case KillsForDetails1Role:
        return row.entry.killsForDetails1;
    case KillsForDetails2Role:
        return row.entry.killsForDetails2;
    case KillsForDetails3Role:
        return row.entry.killsForDetails3;
    case IsFullyUnlockedRole:
        return row.entry.isFullyUnlocked();
    default:
        return {};
    }
}

QHash<int, QByteArray> TCreatureTrackerListModel::roleNames() const
{
    return { { RaceIdRole, "raceID" },
             { NameRole, "name" },
             { CreatureHasCooldownRole, "creatureHasCooldown" },
             { CooldownIsExpiredRole, "cooldownIsExpired" },
             { CooldownEndTextRole, "cooldownEndText" },
             { CurrentKillsRole, "currentKills" },
             { KillsForDetails1Role, "killsForDetails1" },
             { KillsForDetails2Role, "killsForDetails2" },
             { KillsForDetails3Role, "killsForDetails3" },
             { IsFullyUnlockedRole, "isFullyUnlocked" } };
}

void TCreatureTrackerListModel::setRows(std::vector<Row> rows)
{
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
}

TCreatureTrackerWidgetController::TCreatureTrackerWidgetController(const EWidgetMode mode, cyclopedia::TCreatureTrackerStorage* creatures,
                                                                   bossbestiary::TBossTrackerStorage* bosses, QQuickItem* gameWindowItem,
                                                                   QQmlEngine* engine, QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
    , m_mode(mode)
    , m_creatures(creatures)
    , m_bosses(bosses)
    , m_model(new TCreatureTrackerListModel(this))
{
    m_widgetMode = mode;
    m_trackedCreatures = m_model;
    if (m_creatures) {
        if (mode == Mode::Bosstiary)
            connect(m_creatures, &cyclopedia::TCreatureTrackerStorage::trackedBossesChanged, this, &TCreatureTrackerWidgetController::rebuild);
        else
            connect(m_creatures, &cyclopedia::TCreatureTrackerStorage::trackedMonstersChanged, this, &TCreatureTrackerWidgetController::rebuild);
    }
    if (m_bosses && mode == Mode::Bosstiary)
        connect(m_bosses, &bossbestiary::TBossTrackerStorage::bossEntriesChanged, this, &TCreatureTrackerWidgetController::rebuild);
    rebuild();
    widget()->createItem(QStringLiteral("CreatureTrackerWidget.qml"));
}

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

void TCreatureTrackerWidgetController::persistOptions()
{
    if (!m_options)
        return;
    QJsonObject json = m_options->section(QLatin1String(optionsKey(m_mode))).toObject();
    json.insert(QStringLiteral("contentHeight"), widget()->maximizedContentHeight());
    json.insert(QStringLiteral("contentMaximized"), widget()->isMaximized());
    m_options->setSection(QLatin1String(optionsKey(m_mode)), json);
}

void TCreatureTrackerWidgetController::rebuild()
{
    std::vector<TCreatureTrackerListModel::Row> rows;
    if (!m_creatures) {
        m_model->setRows({});
        return;
    }
    const auto& source = m_mode == Mode::Bosstiary ? m_creatures->bosses() : m_creatures->monsters();
    rows.reserve(source.size());
    for (const cyclopedia::TCreatureTrackerEntry& entry : source) {
        TCreatureTrackerListModel::Row row;
        row.entry = entry;
        if (m_mode == Mode::Bosstiary && m_bosses) {
            const qint64 until = m_bosses->cooldownUntil(entry.raceId);
            if (until > 0) {
                row.creatureHasCooldown = true;
                row.cooldownIsExpired = until <= QDateTime::currentSecsSinceEpoch();
                row.cooldownEndText = formatCooldownEnd(until);
            }
        }
        rows.push_back(std::move(row));
    }
    m_model->setRows(std::move(rows));
}

void TCreatureTrackerWidgetController::requestOpenBestiary()
{
    auto action = input::mutableClone(*input::makeAction(A::OPEN_CYCLOPEDIA));
    action->parameter = m_mode == Mode::Bosstiary ? 6 : 1;
    input::publish(action);
}

void TCreatureTrackerWidgetController::requestOpenBestiaryEntry(const uint RaceID)
{
    auto action = input::mutableClone(*input::makeAction(A::OPEN_CYCLOPEDIA));
    action->parameter = m_mode == Mode::Bosstiary ? 6 : 1;
    action->category = static_cast<int>(RaceID);
    input::publish(action);
}

void TCreatureTrackerWidgetController::requestContextMenu(const uint RaceID, const QString& RaceName)
{
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    menu->addCustomEntry(RaceName.isEmpty() ? QString::number(RaceID) : RaceName);
    connect(menu->addCustomEntry(qtTrId("quest_tracker_remove_tracked")), &TContextMenuEntryItem::triggered, this, [RaceID] {
        g_game.sendStatusTrackerBestiary(static_cast<uint16_t>(RaceID), false);
    });
    menu->showCustomMenu();
}

void TCreatureTrackerWidgetController::requestWidgetConfigurationMenu()
{
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    menu->addCustomEntry(m_mode == Mode::Bosstiary ? qtTrId("bosstiary_tracker_caption") : qtTrId("bestiary_tracker_caption"));
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

void TCreatureTrackerWidgetManager::close()
{
    if (!m_controller)
        return;
    m_controller->saveWidgetOptions();
    m_controller->deleteLater();
    m_controller = nullptr;
}

TSidebarWidget* TCreatureTrackerWidgetManager::current() const { return m_controller ? m_controller->widget() : nullptr; }

} // namespace tibia::gamewindow
