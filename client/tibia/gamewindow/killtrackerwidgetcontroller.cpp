#include "killtrackerwidgetcontroller.h"

#include "config/sidebaroptions.h"
#include "gamewindow/preydialogcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <QJsonObject>

namespace tibia::gamewindow {

namespace {

constexpr auto OptionsKey = "preyWidgetOptions";

}

TTaskMonsterModel::TTaskMonsterModel(QObject* parent)
    : QAbstractListModel(parent)
{}

int TTaskMonsterModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant TTaskMonsterModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_entries.size()))
        return {};
    const Entry& e = m_entries[static_cast<size_t>(index.row())];
    switch (role) {
    case NameRole:
        return e.name;
    case CountRole:
        return e.count;
    case RaceIdRole:
        return e.raceId;
    default:
        return {};
    }
}

QHash<int, QByteArray> TTaskMonsterModel::roleNames() const
{
    return { { NameRole, "name" }, { CountRole, "count" }, { RaceIdRole, "raceId" } };
}

void TTaskMonsterModel::setEntries(std::vector<Entry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
}

TKillTrackerWidgetController::TKillTrackerWidgetController(prey::TPreyStorage* storage, QQuickItem* gameWindowItem, QQmlEngine* engine,
                                                           QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
    , m_storage(storage)
{
    m_bountyTasksModel = new TTaskMonsterModel(this);
    m_weeklyTasksModel = new TTaskMonsterModel(this);
    for (prey::TPreyData* data : m_storage->slots())
        m_preyList.append(new TPreyRenderInfo(data, this));
    connect(m_storage, &prey::TPreyStorage::bonusRerollPossibleChanged, this, &TKillTrackerWidgetController::bonusRerollPossibleChanged);
    connect(m_storage, &prey::TPreyStorage::choosePreyPossibleChanged, this, &TKillTrackerWidgetController::lockPreyPossibleChanged);
    widget()->createItem(QStringLiteral("prey/KillTrackerWidget.qml"));
}

bool TKillTrackerWidgetController::bonusRerollpossible() const { return m_storage->bonusRerollPossible(); }

bool TKillTrackerWidgetController::lockPreyPossible() const { return m_storage->choosePreyPossible(); }

void TKillTrackerWidgetController::setSidebarOptions(config::TSidebarOptions* options)
{
    m_options = options;
    if (!m_options)
        return;
    const QJsonObject json = m_options->section(QLatin1String(OptionsKey)).toObject();
    if (json.contains(QLatin1String("contentHeight")))
        widget()->setInitialContentHeightAndMaximized(json.value(QLatin1String("contentHeight")).toInt(),
                                                       json.value(QLatin1String("contentMaximized")).toBool(true));
}

void TKillTrackerWidgetController::requestOpenPreyDialog() { input::publish(input::makeAction(input::EGameAction::OPEN_PREY_DIALOG)); }

void TKillTrackerWidgetController::saveWidgetOptions()
{
    if (!m_options)
        return;
    QJsonObject json = m_options->section(QLatin1String(OptionsKey)).toObject();
    json.insert(QStringLiteral("contentHeight"), widget()->maximizedContentHeight());
    json.insert(QStringLiteral("contentMaximized"), widget()->isMaximized());
    m_options->setSection(QLatin1String(OptionsKey), json);
}

TKillTrackerWidgetManager::TKillTrackerWidgetManager(prey::TPreyStorage* storage, QQuickItem* gameWindowItem, QQmlEngine* engine,
                                                     QObject* parent)
    : QObject(parent)
    , m_storage(storage)
    , m_gameWindowItem(gameWindowItem)
    , m_engine(engine)
{}

TKillTrackerWidgetManager::~TKillTrackerWidgetManager() { delete m_controller; }

TSidebarWidget* TKillTrackerWidgetManager::open(config::TSidebarOptions* options)
{
    if (!m_controller) {
        m_controller = new TKillTrackerWidgetController(m_storage, m_gameWindowItem, m_engine);
        m_controller->setSidebarOptions(options);
    }
    return m_controller->widget();
}

void TKillTrackerWidgetManager::close()
{
    if (!m_controller)
        return;
    m_controller->saveWidgetOptions();
    m_controller->deleteLater();
    m_controller = nullptr;
}

TSidebarWidget* TKillTrackerWidgetManager::current() const { return m_controller ? m_controller->widget() : nullptr; }

}
