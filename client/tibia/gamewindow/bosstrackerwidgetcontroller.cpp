#include "bosstrackerwidgetcontroller.h"

#include "config/sidebaroptions.h"
#include "gamewindow/contextmenucontroller.h"
#include "input/gameactionhandlers.h"
#include "qmlcomponents/outfittypes.h"
#include "qmlcomponents/qmlcomponentsciptypes.h"

#include <tibia/assets/things.h>

#include <QDateTime>
#include <QJsonObject>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;
constexpr auto OptionsKey = "bossCooldownsWidgetOptions";

QString formatCooldownEnd(const qint64 until)
{
    if (until <= 0)
        return {};
    return QDateTime::fromSecsSinceEpoch(until).toString(QStringLiteral("yyyy-MM-dd, HH:mm:ss"));
}

qmlcomponents::TOutfitQmlModel* outfitForRace(const int raceId, QObject* parent)
{
    const RaceType& race = g_things.getRaceData(static_cast<uint32_t>(raceId));
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
    switch (role) {
    case BossRaceIdRole:
        return row.entry.raceId;
    case NameRole:
        return row.entry.name;
    case HasCooldownRole:
        return row.entry.hasCooldown();
    case CooldownEndStringRole:
        return formatCooldownEnd(row.entry.cooldownUntil);
    case OutfitRole:
        return QVariant::fromValue<QObject*>(row.outfit);
    default:
        return {};
    }
}

QHash<int, QByteArray> TBossTrackerListModel::roleNames() const
{
    return { { BossRaceIdRole, "bossRaceId" },
             { NameRole, "name" },
             { HasCooldownRole, "hasCooldown" },
             { CooldownEndStringRole, "cooldownEndString" },
             { OutfitRole, "outfit" } };
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
    m_visible.clear();
    m_visible.reserve(m_all.size());
    for (int i = 0; i < static_cast<int>(m_all.size()); ++i) {
        if (m_filter.isEmpty() || m_all[static_cast<size_t>(i)].entry.name.contains(m_filter, Qt::CaseInsensitive))
            m_visible.push_back(i);
    }
    endResetModel();
}

void TBossTrackerListModel::setNameFilter(const QString& filter)
{
    if (m_filter == filter)
        return;
    beginResetModel();
    m_filter = filter;
    m_visible.clear();
    for (int i = 0; i < static_cast<int>(m_all.size()); ++i) {
        if (m_filter.isEmpty() || m_all[static_cast<size_t>(i)].entry.name.contains(m_filter, Qt::CaseInsensitive))
            m_visible.push_back(i);
    }
    endResetModel();
}

TBossTrackerWidgetController::TBossTrackerWidgetController(bossbestiary::TBossTrackerStorage* storage, QQuickItem* gameWindowItem,
                                                           QQmlEngine* engine, QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
    , m_storage(storage)
    , m_model(new TBossTrackerListModel(this))
{
    m_bosses = m_model;
    if (m_storage) {
        connect(m_storage, &bossbestiary::TBossTrackerStorage::bossEntriesChanged, this, &TBossTrackerWidgetController::rebuild);
        m_hasUnlockedAnyBosses = m_storage->hasUnlockedAnyBosses();
    }
    rebuild();
    widget()->createItem(QStringLiteral("BossTrackingWidget.qml"));
}

void TBossTrackerWidgetController::setSidebarOptions(config::TSidebarOptions* options)
{
    m_options = options;
    if (!m_options)
        return;
    const QJsonObject json = m_options->section(QLatin1String(OptionsKey)).toObject();
    if (json.contains(QLatin1String("contentHeight")))
        widget()->setInitialContentHeightAndMaximized(json.value(QLatin1String("contentHeight")).toInt(),
                                                       json.value(QLatin1String("contentMaximized")).toBool(true));
}

void TBossTrackerWidgetController::saveWidgetOptions() { persistOptions(); }

void TBossTrackerWidgetController::persistOptions()
{
    if (!m_options)
        return;
    QJsonObject json = m_options->section(QLatin1String(OptionsKey)).toObject();
    json.insert(QStringLiteral("contentHeight"), widget()->maximizedContentHeight());
    json.insert(QStringLiteral("contentMaximized"), widget()->isMaximized());
    m_options->setSection(QLatin1String(OptionsKey), json);
}

void TBossTrackerWidgetController::rebuild()
{
    std::vector<TBossTrackerListModel::Row> rows;
    if (m_storage) {
        rows.reserve(m_storage->entries().size());
        for (const bossbestiary::TBossTrackerEntry& entry : m_storage->entries()) {
            TBossTrackerListModel::Row row;
            row.entry = entry;
            row.outfit = outfitForRace(entry.raceId, m_model);
            rows.push_back(std::move(row));
        }
        const bool unlocked = m_storage->hasUnlockedAnyBosses();
        if (unlocked != m_hasUnlockedAnyBosses) {
            m_hasUnlockedAnyBosses = unlocked;
            Q_EMIT bossCountChanged();
        }
    }
    m_model->setRows(std::move(rows));
}

void TBossTrackerWidgetController::requestFilterByBossName(const QString& Filter) { m_model->setNameFilter(Filter); }

void TBossTrackerWidgetController::requestOpenBosstiary(const int BossRaceID)
{
    auto action = input::mutableClone(*input::makeAction(A::OPEN_CYCLOPEDIA));
    action->parameter = 6;
    action->category = BossRaceID;
    input::publish(action);
}

void TBossTrackerWidgetController::requestWidgetConfigurationMenu()
{
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    menu->addCustomEntry(qtTrId("bosstracker_caption"));
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
        m_controller->setSidebarOptions(options);
    }
    return m_controller->widget();
}

void TBossTrackerWidgetManager::close()
{
    if (!m_controller)
        return;
    m_controller->saveWidgetOptions();
    m_controller->deleteLater();
    m_controller = nullptr;
}

TSidebarWidget* TBossTrackerWidgetManager::current() const { return m_controller ? m_controller->widget() : nullptr; }

} // namespace tibia::gamewindow
