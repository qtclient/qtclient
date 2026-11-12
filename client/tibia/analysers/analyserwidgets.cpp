#include "analyserwidgets.h"

#include "analysers/impactwidgets.h"

#include "chat/chatchannel.h"
#include "config/sidebaroptions.h"
#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/genericentertextdialogcontroller.h"
#include "gamewindow/sidebarwidgetsmanager.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "qmlcomponents/texthelper.h"

#include <tibia/game/gamenotify.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QQuickItem>
#include <QStandardPaths>
#include <QVector2D>

#include <algorithm>
#include <cmath>
#include <tibia/game/gamesession.h>

namespace tibia::analysers {

using A = input::EGameAction;
using W = gamewindow::TSidebarWidgetsManager;

namespace format {

namespace {

qmlcomponents::TTextHelperQmlService& textHelper()
{
    static auto* helper = new qmlcomponents::TTextHelperQmlService();
    return *helper;
}

}

QString multi(const qint64 value) { return textHelper().formatNumberWithThousandSeparatorsAndThousandShortcutsAsMultiString(value); }

QString grouped(const qint64 value) { return QLocale(QLocale::English).toString(value); }

QString slotCount(const quint32 count) { return textHelper().formatNumberWithThousandShortcutsAsMultiString(count); }

QString dateTime(const qint64 secs) { return textHelper().formatDateTime(static_cast<int>(secs), true); }

QString minutes(const qint64 minutes)
{
    if (minutes < 60)
        return qtTrId("time_minutes_short").arg(minutes);
    return qtTrId("time_hours_and_minutes_short").arg(minutes / 60).arg(minutes % 60);
}

QString damageTypeName(const int element)
{
    switch (element) {
        case 1: return qtTrId("damagetype_fire");
        case 2: return qtTrId("damagetype_earth");
        case 3: return qtTrId("damagetype_energy");
        case 4: return qtTrId("damagetype_ice");
        case 5: return qtTrId("damagetype_holy");
        case 6: return qtTrId("damagetype_death");
        case 7: return qtTrId("damagetype_healing");
        case 8: return qtTrId("damagetype_drowning");
        case 9: return qtTrId("damagetype_lifeleech");
        case 10: return qtTrId("damagetype_manadrain");
        case 11: return qtTrId("damagetype_agony");
        default: return qtTrId("damagetype_physical");
    }
}

QString damageTypeIcon(const int element)
{
    return QStringLiteral("/images/monster-icon-%1-resist.png").arg(damageTypeName(element).toLower().remove(QLatin1Char(' ')));
}

}

namespace {

bool isPremiumPlayer()
{
    return tibia::game::localPlayerIsPremium();
}

void publishAction(const A type, const QString& text = {}, const int parameter = 0, const int instance = 0)
{
    auto action = input::mutableClone(*input::makeAction(type));
    action->text = text;
    action->parameter = parameter;
    action->instance = instance;
    input::publish(action);
}

/// CIP : ADD_SYSTEM_CHAT_MESSAGE in console colour 30.
void publishSystemMessage(const QString& text)
{
    auto action = input::mutableClone(*input::makeAction(A::ADD_SYSTEM_CHAT_MESSAGE));
    action->text = text;
    action->color = chat::consoleColor(30);
    input::publish(action);
}

QJsonObject readOptions(config::TSidebarOptions* options, const char* key)
{
    return options ? options->section(QLatin1String(key)).toObject() : QJsonObject{};
}

template <class Widget>
void applyContentHeight(Widget* widget, config::TSidebarOptions* options, const char* key)
{
    const QJsonObject json = readOptions(options, key);
    if (json.contains(QLatin1String("contentHeight")))
        widget->setInitialContentHeightAndMaximized(json.value(QLatin1String("contentHeight")).toInt(),
                                                    json.value(QLatin1String("contentMaximized")).toBool(true));
}

template <class Widget>
void storeContentHeight(Widget* widget, config::TSidebarOptions* options, const char* key)
{
    if (!options)
        return;
    QJsonObject json = readOptions(options, key);
    json.insert(QStringLiteral("contentHeight"), widget->maximizedContentHeight());
    json.insert(QStringLiteral("contentMaximized"), widget->isMaximized());
    options->setSection(QLatin1String(key), json);
}

/// CIP : "hh:mmh".
QString sessionLength(const THuntingSessionStorage& storage)
{
    const qint64 minutes = storage.sessionLengthMs() / 60000;
    return QStringLiteral("%1:%2h").arg(minutes / 60, 2, 10, QLatin1Char('0')).arg(minutes % 60, 2, 10, QLatin1Char('0'));
}

/// The XP getters (CIP): "0" without XP, else grouped (`multiString`
/// false) or the multi string.
QString xpString(const qint64 value, const bool multiString)
{
    if (value == 0)
        return QStringLiteral("0");
    return multiString ? format::multi(value) : format::grouped(value);
}

/// CIP (with seconds).
QString dateTime(const qint64 secs) { return format::dateTime(secs); }

QString gaugeDialogNumber(const QString& text) { return text.trimmed(); }

}

// ----------------------------------------------------------------------------------------------
// TAnalyticsSelectorWidgetController

namespace {

constexpr auto SelectorOptionsKey = "analyticsSelectorOptions";

}

TAnalyticsSelectorWidgetController::TAnalyticsSelectorWidgetController(gamewindow::TSidebarWidgetsManager* widgets, QQuickItem* gameWindowItem,
                                                                       QQmlEngine* engine, QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
    , m_widgets(widgets)
{
    for (const int type : { W::BossCooldowns, W::LootTracker, W::XpAnalyser, W::LootAnalyser, W::WasteAnalyser, W::ImpactAnalyser,
                            W::DamageInputAnalyser, W::HuntingSessionAnalyser, W::PartyHuntAnalyser })
        onWidgetVisibility(type, m_widgets && m_widgets->isWidgetOpen(type));
    if (m_widgets) {
        connect(m_widgets, &gamewindow::TSidebarWidgetsManager::widgetVisible, this, &TAnalyticsSelectorWidgetController::onWidgetVisible);
        connect(m_widgets, &gamewindow::TSidebarWidgetsManager::widgetHidden, this, &TAnalyticsSelectorWidgetController::onWidgetHidden);
    }
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onPremiumChange", [this](GameObject*, const std::vector<std::any>&) {
        Q_EMIT premiumTimeChanged();
        return std::any{};
    });
    widget()->createItem(QStringLiteral("AnalyticsSelectorWidget.qml"));
    if (QQuickItem* item = quickItem())
        connect(item, &QObject::destroyed, this, &TAnalyticsSelectorWidgetController::onWidgetQuickItemDestroyed);
}

// b1/7db6f3d35540cd64
void TAnalyticsSelectorWidgetController::setSidebarOptions(config::TSidebarOptions* options)
{
    m_options = options;
    applyContentHeight(widget(), m_options, SelectorOptionsKey);
}

QQuickItem* TAnalyticsSelectorWidgetController::gameWindowQuickItem() const { return widget()->gameWindowItem(); }

void TAnalyticsSelectorWidgetController::onCloseClicked() { onRequestClose(); }
void TAnalyticsSelectorWidgetController::onRequestClose() { Q_EMIT widgetClosed(); }
void TAnalyticsSelectorWidgetController::onRequestMoveToSidebar(const int TargetSidebar)
{
    widget()->requestMoveToSidebar(TargetSidebar);
    // Tip deepen: TSidebarWidget emits its own drag/move; also fire tip moc for SoftHook.
    if (TargetSidebar < 0)
        return;
    Q_EMIT widgetDragChanged(widget()->parentSidebarId(), false);
    Q_EMIT widgetWantsToMoveToSidebar(TargetSidebar, widget());
}
void TAnalyticsSelectorWidgetController::onSetDragActive(const bool Active)
{
    widget()->setDragActive(Active);
    Q_EMIT widgetDragChanged(widget()->parentSidebarId(), Active);
}
void TAnalyticsSelectorWidgetController::onSidebarDestroyed() { widget()->onSidebarDestroyed(); }
void TAnalyticsSelectorWidgetController::onLevelUpClicked() {}
void TAnalyticsSelectorWidgetController::setCursorToResizeVertical(const bool Enabled)
{
    widget()->setCursorToResizeVertical(Enabled);
}
void TAnalyticsSelectorWidgetController::startContentSizeChangedTimer() { widget()->startContentSizeChangedTimer(); }
void TAnalyticsSelectorWidgetController::onWidgetQuickItemDestroyed() {}

bool TAnalyticsSelectorWidgetController::isPremium() const { return isPremiumPlayer(); }

bool TAnalyticsSelectorWidgetController::lootTrackerVisible() const { return m_lootTrackerVisible; }
bool TAnalyticsSelectorWidgetController::lootAnalyserVisible() const { return m_lootAnalyserVisible; }
bool TAnalyticsSelectorWidgetController::wasteAnalyserVisible() const { return m_wasteAnalyserVisible; }
bool TAnalyticsSelectorWidgetController::impactAnalyserVisible() const { return m_impactAnalyserVisible; }
bool TAnalyticsSelectorWidgetController::damageInputAnalyserVisible() const { return m_damageInputAnalyserVisible; }
bool TAnalyticsSelectorWidgetController::progressAnalyserVisible() const { return m_progressAnalyserVisible; }
bool TAnalyticsSelectorWidgetController::huntingSessionAnalyserVisible() const { return m_huntingSessionAnalyserVisible; }
bool TAnalyticsSelectorWidgetController::partyHuntAnalyserVisible() const { return m_partyHuntAnalyserVisible; }
bool TAnalyticsSelectorWidgetController::bossTrackerVisible() const { return m_bossTrackerVisible; }

void TAnalyticsSelectorWidgetController::saveWidgetOptions() { storeContentHeight(widget(), m_options, SelectorOptionsKey); }

void TAnalyticsSelectorWidgetController::onWidgetVisible(const int WidgetType) { onWidgetVisibility(WidgetType, true); }

void TAnalyticsSelectorWidgetController::onWidgetHidden(const int WidgetType) { onWidgetVisibility(WidgetType, false); }

void TAnalyticsSelectorWidgetController::onWidgetOptionsChanged() { saveWidgetOptions(); }

void TAnalyticsSelectorWidgetController::onWidgetVisibility(const int widgetType, const bool visible)
{
    bool* flag = nullptr;
    switch (widgetType) {
        case W::BossCooldowns: flag = &m_bossTrackerVisible; break;
        case W::LootTracker: flag = &m_lootTrackerVisible; break;
        case W::XpAnalyser: flag = &m_progressAnalyserVisible; break;
        case W::LootAnalyser: flag = &m_lootAnalyserVisible; break;
        case W::WasteAnalyser: flag = &m_wasteAnalyserVisible; break;
        case W::ImpactAnalyser: flag = &m_impactAnalyserVisible; break;
        case W::DamageInputAnalyser: flag = &m_damageInputAnalyserVisible; break;
        case W::HuntingSessionAnalyser: flag = &m_huntingSessionAnalyserVisible; break;
        case W::PartyHuntAnalyser: flag = &m_partyHuntAnalyserVisible; break;
        default: return;
    }
    if (*flag == visible)
        return;
    *flag = visible;
    Q_EMIT widgetVisibilityChanged();
}

void TAnalyticsSelectorWidgetController::toggle(const int widgetType)
{
    auto action = input::mutableClone(*input::makeAction(A::TOGGLE_WIDGET));
    action->instance = widgetType;
    input::publish(action);
}

void TAnalyticsSelectorWidgetController::lootTrackerButtonClicked() { toggle(W::LootTracker); }
void TAnalyticsSelectorWidgetController::lootAnalyserButtonClicked() { toggle(W::LootAnalyser); }
void TAnalyticsSelectorWidgetController::wasteAnalyserButtonClicked() { toggle(W::WasteAnalyser); }
void TAnalyticsSelectorWidgetController::impactAnalyserButtonClicked() { toggle(W::ImpactAnalyser); }
void TAnalyticsSelectorWidgetController::damageInputAnalyserButtonClicked() { toggle(W::DamageInputAnalyser); }
void TAnalyticsSelectorWidgetController::progressAnalyserButtonClicked() { toggle(W::XpAnalyser); }
void TAnalyticsSelectorWidgetController::partyHuntAnalyserButtonClicked() { toggle(W::PartyHuntAnalyser); }
void TAnalyticsSelectorWidgetController::bossTrackerButtonClicked() { toggle(W::BossCooldowns); }

/// Free accounts get the store's premium page (OPEN_STORE 1 / 0) instead.
void TAnalyticsSelectorWidgetController::huntingSessionAnalyserButtonClicked()
{
    if (isPremiumPlayer())
        toggle(W::HuntingSessionAnalyser);
    else
        publishAction(A::OPEN_STORE, {}, 1, 0);
}

// ----------------------------------------------------------------------------------------------
// THuntingSessionWidgetController

namespace {

constexpr auto HuntingOptionsKey = "huntingSessionAnalyserWidgetOptions";

QStringList prefixedEntries(const char* entryId, const std::vector<std::pair<int, QString>>& entries)
{
    QStringList list;
    list.reserve(static_cast<qsizetype>(entries.size()));
    const QString prefix = qtTrId("subentry_prefix");
    for (const auto& [count, name] : entries)
        list.append(prefix.arg(qtTrId(entryId).arg(count).arg(name)));
    return list;
}

}

THuntingSessionWidgetController::THuntingSessionWidgetController(TAnalyserStorages* storages, QQuickItem* gameWindowItem, QQmlEngine* engine,
                                                                 QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
    , m_storages(storages)
{
    m_lootedTimer.setSingleShot(true);
    m_lootedTimer.setInterval(250);
    connect(&m_lootedTimer, &QTimer::timeout, this, &THuntingSessionWidgetController::lootedItemsChanged);

    THuntingSessionStorage* session = m_storages->huntingSession();
    connect(session, &THuntingSessionStorage::sessionLengthChanged, this, [this] {
        Q_EMIT sessionLengthChanged();
        Q_EMIT experienceChanged();
        Q_EMIT damageOrHealingChanged();
    });
    connect(session, &THuntingSessionStorage::experienceChanged, this, &THuntingSessionWidgetController::experienceChanged);
    connect(session, &THuntingSessionStorage::killsChanged, this, &THuntingSessionWidgetController::killedMonstersChanged);
    connect(session, &THuntingSessionStorage::optionsChanged, this, &THuntingSessionWidgetController::characterConfigurationChanged);
    connect(m_storages->progress(), &TProgressStorage::dataChanged, this, &THuntingSessionWidgetController::experienceChanged);
    connect(m_storages->gainWaste(), &TGainWasteStorage::metricsChanged, this, &THuntingSessionWidgetController::gainOrWasteChanged);
    connect(m_storages->gainWaste(), &TGainWasteStorage::itemsChanged, this, [this](const int side) {
        if (side == TGainWasteStorage::Loot && !m_lootedTimer.isActive())
            m_lootedTimer.start();
    });
    connect(m_storages->impact(), &TImpactStorage::damageValuesChanged, this, &THuntingSessionWidgetController::damageOrHealingChanged);
    connect(m_storages->impact(), &TImpactStorage::healingValuesChanged, this, &THuntingSessionWidgetController::damageOrHealingChanged);
    connect(m_storages, &TAnalyserStorages::sessionReset, this, [this] {
        Q_EMIT sessionLengthChanged();
        Q_EMIT experienceChanged();
        Q_EMIT gainOrWasteChanged();
        Q_EMIT damageOrHealingChanged();
        Q_EMIT killedMonstersChanged();
        Q_EMIT lootedItemsChanged();
    });
    widget()->createItem(QStringLiteral("HuntingSessionWidget.qml"));
    if (QQuickItem* item = quickItem())
        connect(item, &QObject::destroyed, this, &THuntingSessionWidgetController::onWidgetQuickItemDestroyed);
}

void THuntingSessionWidgetController::setSidebarOptions(config::TSidebarOptions* options)
{
    m_options = options;
    applyContentHeight(widget(), m_options, HuntingOptionsKey);
}

QQuickItem* THuntingSessionWidgetController::gameWindowQuickItem() const { return widget()->gameWindowItem(); }

void THuntingSessionWidgetController::onCloseClicked() { onRequestClose(); }
void THuntingSessionWidgetController::onRequestClose() { Q_EMIT widgetClosed(); }
void THuntingSessionWidgetController::onRequestMoveToSidebar(const int TargetSidebar)
{
    widget()->requestMoveToSidebar(TargetSidebar);
    if (TargetSidebar < 0)
        return;
    Q_EMIT widgetDragChanged(widget()->parentSidebarId(), false);
    Q_EMIT widgetWantsToMoveToSidebar(TargetSidebar, widget());
}
void THuntingSessionWidgetController::onSetDragActive(const bool Active)
{
    widget()->setDragActive(Active);
    Q_EMIT widgetDragChanged(widget()->parentSidebarId(), Active);
}
void THuntingSessionWidgetController::onSidebarDestroyed() { widget()->onSidebarDestroyed(); }
void THuntingSessionWidgetController::onLevelUpClicked() {}
void THuntingSessionWidgetController::setCursorToResizeVertical(const bool Enabled)
{
    widget()->setCursorToResizeVertical(Enabled);
}
void THuntingSessionWidgetController::startContentSizeChangedTimer() { widget()->startContentSizeChangedTimer(); }
void THuntingSessionWidgetController::onWidgetQuickItemDestroyed() {}

void THuntingSessionWidgetController::saveWidgetOptions() { storeContentHeight(widget(), m_options, HuntingOptionsKey); }

void THuntingSessionWidgetController::onWidgetOptionsChanged() { saveWidgetOptions(); }

bool THuntingSessionWidgetController::showBaseXp() const { return m_storages->huntingSession()->showBaseXp(); }

QString THuntingSessionWidgetController::sessionLengthString() const { return sessionLength(*m_storages->huntingSession()); }

/// CIP : the session total over the session length, per hour.
qint64 THuntingSessionWidgetController::perHour(const qint64 total) const
{
    const qint64 length = m_storages->huntingSession()->sessionLengthMs();
    if (length == 0)
        return 0;
    return static_cast<qint64>(static_cast<double>(total) / static_cast<double>(length) * 1000.0 * 3600.0);
}

QString THuntingSessionWidgetController::accumulatedXpString() const { return xpString(m_storages->progress()->finalXp().total(), true); }

QString THuntingSessionWidgetController::xpPerHourString() const { return xpString(perHour(m_storages->progress()->finalXp().total()), true); }

QString THuntingSessionWidgetController::baseAccumulatedXpString() const { return xpString(m_storages->progress()->baseXp().total(), true); }

QString THuntingSessionWidgetController::baseXpPerHourString() const { return xpString(perHour(m_storages->progress()->baseXp().total()), true); }

/// CIP.
QString THuntingSessionWidgetController::gainString() const { return xpString(m_storages->gainWaste()->sums(TGainWasteStorage::Loot).session(), true); }

QString THuntingSessionWidgetController::wasteString() const { return xpString(m_storages->gainWaste()->sums(TGainWasteStorage::Waste).session(), true); }

/// CIP .
qlonglong THuntingSessionWidgetController::gainWasteSum() const
{
    return m_storages->gainWaste()->sums(TGainWasteStorage::Loot).session() - m_storages->gainWaste()->sums(TGainWasteStorage::Waste).session();
}

QString THuntingSessionWidgetController::gainWasteSumString() const { return xpString(gainWasteSum(), true); }

/// CIP / : the impact storage's session totals.
QString THuntingSessionWidgetController::damageString() const { return xpString(m_storages->impact()->session(TImpactStorage::Damage).total, true); }

QString THuntingSessionWidgetController::healingString() const { return xpString(m_storages->impact()->session(TImpactStorage::Healing).total, true); }

/// CIP / : the session per second value per hour, cut to 32 bits.
QString THuntingSessionWidgetController::damagePerHourString() const
{
    const auto value = static_cast<quint32>(static_cast<qint64>(m_storages->impact()->sessionPerSecond(TImpactStorage::Damage) * 3600.0));
    return xpString(value, true);
}

QString THuntingSessionWidgetController::healingPerHourString() const
{
    const auto value = static_cast<quint32>(static_cast<qint64>(m_storages->impact()->sessionPerSecond(TImpactStorage::Healing) * 3600.0));
    return xpString(value, true);
}

/// CIP : the session's loot, "count name" by item id.
QStringList THuntingSessionWidgetController::lootedItems() const
{
    std::vector<std::pair<int, QString>> entries;
    for (const TGainWasteStorage::Item& item : m_storages->gainWaste()->sessionItems(TGainWasteStorage::Loot))
        entries.emplace_back(static_cast<int>(item.count), item.name);
    return prefixedEntries("huntinganalyser_item", entries);
}

/// CIP .
QStringList THuntingSessionWidgetController::killedMonsters() const
{
    std::vector<std::pair<int, QString>> entries;
    for (const auto& [name, count] : m_storages->huntingSession()->kills())
        entries.emplace_back(count, name);
    return prefixedEntries("huntinganalyser_monster", entries);
}

/// CIP .
void THuntingSessionWidgetController::requestConfigurationContextMenu()
{
    auto* menu = gamewindow::TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    connect(menu->addCustomEntry(qtTrId("huntinganalyser_reset_session")), &gamewindow::TContextMenuEntryItem::triggered, this,
            [] { publishAction(A::RESET_HUNTING_SESSION); });
    menu->addCustomSeparator();
    gamewindow::TContextMenuEntryItem* baseXp = menu->addCustomEntry(qtTrId("huntinganalyser_show_base_xp"));
    baseXp->setCheckable(true);
    baseXp->setChecked(showBaseXp());
    connect(baseXp, &gamewindow::TContextMenuEntryItem::toggled, this,
            [this](const bool checked) { m_storages->huntingSession()->setShowBaseXp(checked); });
    menu->addCustomSeparator();
    connect(menu->addCustomEntry(qtTrId("copy_to_clipboard")), &gamewindow::TContextMenuEntryItem::triggered, this,
            [this] { publishAction(A::COPY_TRUSTED_TEXT_TO_CLIPBOARD, exportText()); });
    connect(menu->addCustomEntry(qtTrId("huntinganalyser_save_data_to_file")), &gamewindow::TContextMenuEntryItem::triggered, this, [this] {
        const QString path = exportFileName("txt");
        auto action = input::mutableClone(*input::makeAction(A::SAVE_TEXT_TO_FILE));
        action->text = exportText();
        action->filePath = path;
        input::publish(action);
        publishSystemMessage(qtTrId("huntinganalyser_session_saved").arg(path));
    });
    connect(menu->addCustomEntry(qtTrId("huntinganalyser_save_data_to_json")), &gamewindow::TContextMenuEntryItem::triggered, this, [this] {
        const QString path = exportFileName("json");
        auto action = input::mutableClone(*input::makeAction(A::SAVE_JSON_TO_FILE));
        action->text = QString::fromUtf8(QJsonDocument(exportJson()).toJson());
        action->filePath = path;
        input::publish(action);
        publishSystemMessage(qtTrId("huntinganalyser_session_saved").arg(path));
    });
    menu->showCustomMenu();
}

/// CIP : <app data>/log/ (TTibiaFileSystemHelper category 5) and
/// huntinganalyser_session_export_file_name with "yyyy-MM-dd_HHmmss".
QString THuntingSessionWidgetController::exportFileName(const char* extension) const
{
    const QString name = qtTrId("huntinganalyser_session_export_file_name")
                             .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HHmmss")))
                             .arg(QLatin1String(extension));
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/log")).filePath(name);
}

/// CIP : "label: value" lines with grouped numbers, then the kills and the loot.
// b1/8a1a57734a40c2fa
QString THuntingSessionWidgetController::exportText() const
{
    QStringList lines;
    const THuntingSessionStorage* session = m_storages->huntingSession();
    const TProgressStorage* progress = m_storages->progress();
    const auto line = [&lines](const QString& label, const QString& value) { lines.append(QStringLiteral("%1: %2").arg(label, value)); };
    lines.append(qtTrId("huntinganalyser_session_time")
                     .arg(dateTime(session->sessionStartSecs()))
                     .arg(dateTime(QDateTime::currentSecsSinceEpoch())));
    line(qtTrId("huntinganalyser_sessionlength"), sessionLength(*session));
    if (showBaseXp())
        line(qtTrId("huntinganalyser_base_xpgain"), xpString(progress->baseXp().total(), false));
    line(qtTrId("huntinganalyser_xpgain"), xpString(progress->finalXp().total(), false));
    if (showBaseXp())
        line(qtTrId("huntinganalyser_base_xp_per_hour"), xpString(perHour(progress->baseXp().total()), false));
    line(qtTrId("huntinganalyser_xp_per_hour"), xpString(perHour(progress->finalXp().total()), false));
    line(qtTrId("huntinganalyser_gain"), xpString(m_storages->gainWaste()->sums(TGainWasteStorage::Loot).session(), false));
    line(qtTrId("huntinganalyser_waste"), xpString(m_storages->gainWaste()->sums(TGainWasteStorage::Waste).session(), false));
    line(qtTrId("huntinganalyser_gain_waste_sum"), xpString(gainWasteSum(), false));
    line(qtTrId("damage"), xpString(m_storages->impact()->session(TImpactStorage::Damage).total, false));
    line(qtTrId("huntinganalyser_dph"),
         xpString(static_cast<quint32>(static_cast<qint64>(m_storages->impact()->sessionPerSecond(TImpactStorage::Damage) * 3600.0)), false));
    line(qtTrId("healing"), xpString(m_storages->impact()->session(TImpactStorage::Healing).total, false));
    line(qtTrId("huntinganalyser_hph"),
         xpString(static_cast<quint32>(static_cast<qint64>(m_storages->impact()->sessionPerSecond(TImpactStorage::Healing) * 3600.0)), false));
    lines.append(qtTrId("huntinganalyser_killed_monsters") + QLatin1Char(':'));
    const QStringList kills = killedMonsters();
    if (kills.isEmpty())
        lines.append(qtTrId("subentry_prefix").arg(qtTrId("huntinganalyser_no_kills")));
    else
        lines.append(kills);
    lines.append(qtTrId("huntinganalyser_looted_items") + QLatin1Char(':'));
    const QStringList loot = lootedItems();
    if (loot.isEmpty())
        lines.append(qtTrId("subentry_prefix").arg(qtTrId("huntinganalyser_no_loot")));
    else
        lines.append(loot);
    return lines.join(QLatin1Char('\n'));
}

/// CIP .
// b1/35973c9d29b3061f
QJsonObject THuntingSessionWidgetController::exportJson() const
{
    QJsonObject json;
    const THuntingSessionStorage* session = m_storages->huntingSession();
    const TProgressStorage* progress = m_storages->progress();
    json.insert(qtTrId("huntinganalyser_session_start"), dateTime(session->sessionStartSecs()));
    json.insert(qtTrId("huntinganalyser_session_end"), dateTime(QDateTime::currentSecsSinceEpoch()));
    json.insert(qtTrId("huntinganalyser_session_length"), sessionLength(*session));
    json.insert(qtTrId("huntinganalyser_xpgain"), xpString(progress->finalXp().total(), false));
    json.insert(qtTrId("huntinganalyser_xp_per_hour"), xpString(perHour(progress->finalXp().total()), false));
    if (showBaseXp()) {
        json.insert(qtTrId("huntinganalyser_base_xpgain"), xpString(progress->baseXp().total(), false));
        json.insert(qtTrId("huntinganalyser_base_xp_per_hour"), xpString(perHour(progress->baseXp().total()), false));
    }
    json.insert(qtTrId("huntinganalyser_gain"), xpString(m_storages->gainWaste()->sums(TGainWasteStorage::Loot).session(), false));
    json.insert(qtTrId("huntinganalyser_waste"), xpString(m_storages->gainWaste()->sums(TGainWasteStorage::Waste).session(), false));
    json.insert(qtTrId("huntinganalyser_gain_waste_sum"), xpString(gainWasteSum(), false));
    json.insert(qtTrId("damage"), xpString(m_storages->impact()->session(TImpactStorage::Damage).total, false));
    json.insert(qtTrId("huntinganalyser_dph"),
                xpString(static_cast<quint32>(static_cast<qint64>(m_storages->impact()->sessionPerSecond(TImpactStorage::Damage) * 3600.0)), false));
    json.insert(qtTrId("healing"), xpString(m_storages->impact()->session(TImpactStorage::Healing).total, false));
    json.insert(qtTrId("huntinganalyser_hph"),
                xpString(static_cast<quint32>(static_cast<qint64>(m_storages->impact()->sessionPerSecond(TImpactStorage::Healing) * 3600.0)), false));
    QJsonArray kills;
    for (const auto& [name, count] : session->kills())
        kills.append(QJsonObject{ { qtTrId("name"), name }, { qtTrId("Count"), count } });
    json.insert(qtTrId("huntinganalyser_killed_monsters"), kills);
    QJsonArray loot;
    for (const TGainWasteStorage::Item& item : m_storages->gainWaste()->sessionItems(TGainWasteStorage::Loot))
        loot.append(QJsonObject{ { qtTrId("name"), item.name }, { qtTrId("Count"), static_cast<qint64>(item.count) } });
    json.insert(qtTrId("huntinganalyser_looted_items"), loot);
    return json;
}

// ----------------------------------------------------------------------------------------------
// TProgressWidgetController

namespace {

constexpr auto ProgressOptionsKey = "xpAnalyserWidgetOptions";
constexpr int HistoryPoints = 60;

QVariantList emptyHistory()
{
    QVariantList history;
    history.reserve(HistoryPoints);
    for (int x = -HistoryPoints; x != 0; ++x)
        history.append(QVector2D(static_cast<float>(x), 0.0f));
    return history;
}

}

TProgressWidgetController::TProgressWidgetController(TProgressStorage* storage, QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
    , m_storage(storage)
{
    m_xpHistory = emptyHistory();
    m_xpGraphMaxY = 1.0;
    connect(m_storage, &TProgressStorage::dataChanged, this, &TProgressWidgetController::onExperienceInProgressStorageChanged);
    connect(m_storage, &TProgressStorage::wasReset, this, [this] {
        m_lastTexts = m_lastGauge = m_lastGraph = 0;
        onExperienceInProgressStorageChanged();
    });
    connect(m_storage, &TProgressStorage::optionsChanged, this, &TProgressWidgetController::optionsChanged);
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onPremiumChange", [this](GameObject*, const std::vector<std::any>&) {
        Q_EMIT optionsChanged();
        return std::any{};
    });
    for (const char* event : { "onExperienceChange", "onLevelChange" }) {
        m_hooks.emplace_back(tibia::game::kNotifyScope, event, [this](GameObject*, const std::vector<std::any>&) {
            Q_EMIT experienceDataChangedForTexts();
            return std::any{};
        });
    }
    updateXPGraphData();
    widget()->createItem(QStringLiteral("ProgressWidget.qml"));
    if (QQuickItem* item = quickItem())
        connect(item, &QObject::destroyed, this, &TProgressWidgetController::onWidgetQuickItemDestroyed);
}

TProgressWidgetController::~TProgressWidgetController() { delete m_targetDialog; }

void TProgressWidgetController::setSidebarOptions(config::TSidebarOptions* options)
{
    m_options = options;
    applyContentHeight(widget(), m_options, ProgressOptionsKey);
}

QQuickItem* TProgressWidgetController::gameWindowQuickItem() const { return widget()->gameWindowItem(); }

void TProgressWidgetController::onCloseClicked() { onRequestClose(); }
void TProgressWidgetController::onRequestClose() { Q_EMIT widgetClosed(); }
void TProgressWidgetController::onRequestMoveToSidebar(const int TargetSidebar)
{
    widget()->requestMoveToSidebar(TargetSidebar);
    if (TargetSidebar < 0)
        return;
    Q_EMIT widgetDragChanged(widget()->parentSidebarId(), false);
    Q_EMIT widgetWantsToMoveToSidebar(TargetSidebar, widget());
}
void TProgressWidgetController::onSetDragActive(const bool Active)
{
    widget()->setDragActive(Active);
    Q_EMIT widgetDragChanged(widget()->parentSidebarId(), Active);
}
void TProgressWidgetController::onSidebarDestroyed() { widget()->onSidebarDestroyed(); }
void TProgressWidgetController::onLevelUpClicked() {}
void TProgressWidgetController::setCursorToResizeVertical(const bool Enabled)
{
    widget()->setCursorToResizeVertical(Enabled);
}
void TProgressWidgetController::startContentSizeChangedTimer() { widget()->startContentSizeChangedTimer(); }
void TProgressWidgetController::onWidgetQuickItemDestroyed() {}

void TProgressWidgetController::saveWidgetOptions() { storeContentHeight(widget(), m_options, ProgressOptionsKey); }

void TProgressWidgetController::onWidgetOptionsChanged() { saveWidgetOptions(); }

// b1/822d579f59d34f23
void TProgressWidgetController::onExperienceInProgressStorageChanged()
{
    const qint64 now = TAnalyserStorages::nowMs();
    if (now - m_lastTexts >= 1000) {
        m_lastTexts = now;
        Q_EMIT experienceDataChangedForTexts();
    }
    if (now - m_lastGauge >= 50) {
        m_lastGauge = now;
        Q_EMIT experienceDataChangedForGauge();
    }
    if (now - m_lastGraph >= 10000) {
        m_lastGraph = now;
        updateXPGraphData();
    }
}

// b1/f4bbefa330208d48
void TProgressWidgetController::updateXPGraphData()
{
    m_storage->computeHistory(m_xpHistory, m_xpGraphMaxY);
    Q_EMIT experienceDataChangedForGraph();
}

bool TProgressWidgetController::isGaugeVisible() const { return m_storage->gaugeVisible() && isPremiumPlayer(); }

bool TProgressWidgetController::isGraphVisible() const { return m_storage->graphVisible() && isPremiumPlayer(); }

double TProgressWidgetController::xpGaugeTargetValue() const { return m_storage->gaugeTarget() * 3600.0; }

QString TProgressWidgetController::xpGaugeTargetValueString() const { return format::multi(static_cast<qint64>(xpGaugeTargetValue())); }

bool TProgressWidgetController::showBaseXp() const { return m_storage->showBaseXp(); }

double TProgressWidgetController::xpGaugeValue() const { return m_storage->finalXp().perSecond() * 3600.0; }

QVariantList TProgressWidgetController::xpHistory() const { return m_xpHistory; }

double TProgressWidgetController::xpGraphMaxY() const { return m_xpGraphMaxY; }

QString TProgressWidgetController::accumulatedXpString() const { return xpString(m_storage->finalXp().total(), true); }

QString TProgressWidgetController::xpPerHourString() const { return xpString(static_cast<qint64>(m_storage->finalXp().perHour()), true); }

QString TProgressWidgetController::baseAccumulatedXpString() const { return xpString(m_storage->baseXp().total(), true); }

QString TProgressWidgetController::baseXpPerHourString() const { return xpString(static_cast<qint64>(m_storage->baseXp().perHour()), true); }

namespace {

/// CIP's experience formula (the server's Player::getExpForLevel).
qint64 experienceForLevel(const qint64 level)
{
    const qint64 l = level - 1;
    return ((50 * l * l * l) - (150 * l * l) + (400 * l)) / 3;
}

qint64 experienceToNextLevel()
{
    if (!tibia::game::localPlayer())
        return 0;
    const int level = tibia::game::localPlayerLevel();
    return std::max<qint64>(0, experienceForLevel(level + 1) - static_cast<qint64>(tibia::game::localPlayerExperience()));
}

}

QString TProgressWidgetController::experiencePointsNeededForNextLevel() const { return format::grouped(experienceToNextLevel()); }

double TProgressWidgetController::playerLevelProgress() const
{
    return tibia::game::localPlayer() ? tibia::game::localPlayerLevelPercent() / 100.0 : 0.0;
}

/// CIP: the XP needed over the XP per minute, below 6000 minutes.
QString TProgressWidgetController::timeToNextLevel() const
{
    const double perMinute = m_storage->finalXp().perSecond() * 60.0;
    if (perMinute <= 0.0)
        return QStringLiteral("-");
    const double minutes = static_cast<double>(experienceToNextLevel()) / perMinute;
    if (minutes >= 6000.0)
        return QStringLiteral("-");
    return format::minutes(static_cast<qint64>(minutes));
}

/// CIP .
void TProgressWidgetController::onShowContextMenu()
{
    auto* menu = gamewindow::TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    connect(menu->addCustomEntry(qtTrId("progress_reset")), &gamewindow::TContextMenuEntryItem::triggered, this, [this] { m_storage->reset(); });
    menu->addCustomSeparator();
    gamewindow::TContextMenuEntryItem* baseXp = menu->addCustomEntry(qtTrId("progress_show_base_xp"));
    baseXp->setCheckable(true);
    baseXp->setChecked(m_storage->showBaseXp());
    connect(baseXp, &gamewindow::TContextMenuEntryItem::toggled, this, [this](const bool checked) { m_storage->setShowBaseXp(checked); });
    menu->addCustomSeparator();
    appendXPGaugeContextMenuEntries();
    appendXPGraphContextMenuEntries();
    menu->showCustomMenu();
}

void TProgressWidgetController::onShowXPGaugeContextMenu()
{
    auto* menu = gamewindow::TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    appendXPGaugeContextMenuEntries();
    menu->showCustomMenu();
}

void TProgressWidgetController::onShowXPGraphContextMenu()
{
    auto* menu = gamewindow::TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    appendXPGraphContextMenuEntries();
    menu->showCustomMenu();
}

/// CIP : the target (premium only) and the gauge switch.
void TProgressWidgetController::appendXPGaugeContextMenuEntries()
{
    auto* menu = gamewindow::TContextMenuController::instance();
    const bool premium = isPremiumPlayer();
    if (premium) {
        connect(menu->addCustomEntry(qtTrId("progress_set_gauge_target")), &gamewindow::TContextMenuEntryItem::triggered, this, [this] {
            if (!m_targetDialog) {
                m_targetDialog = new gamewindow::TGenericEnterTextDialogController();
                m_targetDialog->setCaption(qtTrId("progress_gaugetarget_caption"));
                m_targetDialog->setDescription(qtTrId("progress_gaugetarget_description"));
                m_targetDialog->setMaxInputLength(9);
                connect(m_targetDialog, &gamewindow::TGenericEnterTextDialogController::textEntered, this,
                        &TProgressWidgetController::onXPGaugeTargetValueSelected);
            }
            m_targetDialog->setText(QString::number(static_cast<qint64>(xpGaugeTargetValue())));
            m_targetDialog->dialog()->show();
        });
    }
    gamewindow::TContextMenuEntryItem* gauge =
        menu->addCustomEntry(qtTrId("progress_show_gauge"), premium ? QString() : qtTrId("context_premium_feature"));
    gauge->setCheckable(true);
    gauge->setChecked(m_storage->gaugeVisible() && premium);
    connect(gauge, &gamewindow::TContextMenuEntryItem::toggled, this, [this](const bool checked) { m_storage->setGaugeVisible(checked); });
}

/// CIP .
void TProgressWidgetController::appendXPGraphContextMenuEntries()
{
    auto* menu = gamewindow::TContextMenuController::instance();
    const bool premium = isPremiumPlayer();
    gamewindow::TContextMenuEntryItem* graph =
        menu->addCustomEntry(qtTrId("progress_show_graph"), premium ? QString() : qtTrId("context_premium_feature"));
    graph->setCheckable(true);
    graph->setChecked(m_storage->graphVisible() && premium);
    connect(graph, &gamewindow::TContextMenuEntryItem::toggled, this, [this](const bool checked) { m_storage->setGraphVisible(checked); });
}

/// The entered XP per hour; the storage keeps it per second.
void TProgressWidgetController::onXPGaugeTargetValueSelected(const QString& GaugeTargetString)
{
    bool ok = false;
    const double value = gaugeDialogNumber(GaugeTargetString).toDouble(&ok);
    if (ok && value > 0.0)
        m_storage->setGaugeTarget(value / 3600.0);
}

// ----------------------------------------------------------------------------------------------
// TGainWasteModel

int TGainWasteModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : static_cast<int>(m_items.size()); }

QVariant TGainWasteModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_items.size()))
        return {};
    const TGainWasteStorage::Item& item = m_items[static_cast<size_t>(index.row())];
    switch (role) {
        case ItemName: return item.name;
        case ItemValueString: return format::grouped(item.price);
        case ItemValueForCountString: return format::grouped(item.price * item.count);
        case ItemCountString: return format::slotCount(item.count);
        case AppearanceId: return item.itemId;
        case ItemCount: return std::min<quint32>(item.count, 255);
        case ItemLiquidType: return 0;
        case ItemHookDirection: return 0;
        default: return {};
    }
}

QHash<int, QByteArray> TGainWasteModel::roleNames() const
{
    return {
        { ItemName, "itemName" },
        { ItemValueString, "itemValueString" },
        { ItemValueForCountString, "itemValueForCountString" },
        { ItemCountString, "itemCountString" },
        { AppearanceId, "appearanceId" },
        { ItemCount, "itemCount" },
        { ItemLiquidType, "itemLiquidType" },
        { ItemHookDirection, "itemHookDirection" },
    };
}

// b1/1189aff411a4e991
void TGainWasteModel::setItems(std::vector<TGainWasteStorage::Item> items)
{
    size_t row = 0;
    size_t from = 0;
    while (row < m_items.size() || from < items.size()) {
        if (from >= items.size() || (row < m_items.size() && m_items[row].itemId < items[from].itemId)) {
            beginRemoveRows({}, static_cast<int>(row), static_cast<int>(row));
            m_items.erase(m_items.begin() + static_cast<std::ptrdiff_t>(row));
            endRemoveRows();
        } else if (row >= m_items.size() || items[from].itemId < m_items[row].itemId) {
            beginInsertRows({}, static_cast<int>(row), static_cast<int>(row));
            m_items.insert(m_items.begin() + static_cast<std::ptrdiff_t>(row), std::move(items[from]));
            endInsertRows();
            ++row;
            ++from;
        } else {
            TGainWasteStorage::Item& current = m_items[row];
            if (current.count != items[from].count || current.price != items[from].price || current.name != items[from].name) {
                current = std::move(items[from]);
                const QModelIndex changed = index(static_cast<int>(row));
                Q_EMIT dataChanged(changed, changed);
            }
            ++row;
            ++from;
        }
    }
}

// ----------------------------------------------------------------------------------------------
// TLootAndWasteWidgetController

namespace {

const char* lootWasteOptionsKey(const TGainWasteStorage::Side side)
{
    return side == TGainWasteStorage::Loot ? "lootAnalyserWidgetOptions" : "supplyAnalyserWidgetOptions";
}

}

TLootAndWasteWidgetController::TLootAndWasteWidgetController(TGainWasteStorage* storage, const TGainWasteStorage::Side side,
                                                             QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
    , m_storage(storage)
    , m_side(side)
    , m_model(new TGainWasteModel(this))
{
    m_itemsTimer.setSingleShot(true);
    m_itemsTimer.setInterval(1);
    connect(&m_itemsTimer, &QTimer::timeout, this, [this] { m_model->setItems(m_storage->lastHourItems(m_side)); });
    connect(m_storage, &TGainWasteStorage::itemsChanged, this, [this](const int changedSide) {
        if (changedSide == m_side)
            onItemsChanged();
    });
    connect(m_storage, &TGainWasteStorage::metricsChanged, this, &TLootAndWasteWidgetController::onItemsMetricsChanged);
    connect(m_storage, &TGainWasteStorage::optionsChanged, this, &TLootAndWasteWidgetController::onCharacterBasedLootWasteOptionsChanged);
    m_model->setItems(m_storage->lastHourItems(m_side));
    onItemsMetricsChanged();
    widget()->createItem(QStringLiteral("LootAndWasteWidget.qml"));
    if (QQuickItem* item = quickItem())
        connect(item, &QObject::destroyed, this, &TLootAndWasteWidgetController::onWidgetQuickItemDestroyed);
}

TLootAndWasteWidgetController::~TLootAndWasteWidgetController() { delete m_targetDialog; }

void TLootAndWasteWidgetController::setSidebarOptions(config::TSidebarOptions* options)
{
    m_options = options;
    applyContentHeight(widget(), m_options, lootWasteOptionsKey(m_side));
}

QQuickItem* TLootAndWasteWidgetController::gameWindowQuickItem() const { return widget()->gameWindowItem(); }

void TLootAndWasteWidgetController::onCloseClicked() { onRequestClose(); }
void TLootAndWasteWidgetController::onRequestClose() { Q_EMIT widgetClosed(); }
void TLootAndWasteWidgetController::onRequestMoveToSidebar(const int TargetSidebar)
{
    widget()->requestMoveToSidebar(TargetSidebar);
    if (TargetSidebar < 0)
        return;
    Q_EMIT widgetDragChanged(widget()->parentSidebarId(), false);
    Q_EMIT widgetWantsToMoveToSidebar(TargetSidebar, widget());
}
void TLootAndWasteWidgetController::onSetDragActive(const bool Active)
{
    widget()->setDragActive(Active);
    Q_EMIT widgetDragChanged(widget()->parentSidebarId(), Active);
}
void TLootAndWasteWidgetController::onSidebarDestroyed() { widget()->onSidebarDestroyed(); }
void TLootAndWasteWidgetController::onLevelUpClicked() {}
void TLootAndWasteWidgetController::setCursorToResizeVertical(const bool Enabled)
{
    widget()->setCursorToResizeVertical(Enabled);
}
void TLootAndWasteWidgetController::startContentSizeChangedTimer() { widget()->startContentSizeChangedTimer(); }
void TLootAndWasteWidgetController::onWidgetQuickItemDestroyed() {}

void TLootAndWasteWidgetController::saveWidgetOptions() { storeContentHeight(widget(), m_options, lootWasteOptionsKey(m_side)); }

void TLootAndWasteWidgetController::onItemsChanged()
{
    if (!m_itemsTimer.isActive())
        m_itemsTimer.start();
}

/// The history points: the first at -n, then one per minute ending at 0.
// b1/bcf88103dfa1c3ed
void TLootAndWasteWidgetController::onItemsMetricsChanged()
{
    const std::vector<double> values = m_storage->history(m_side, m_maxY);
    m_history.clear();
    const auto n = static_cast<int>(values.size());
    if (n > 0) {
        m_history.reserve(n + 1);
        m_history.append(QVector2D(static_cast<float>(-n), static_cast<float>(values.front())));
        for (int i = 0; i < n; ++i)
            m_history.append(QVector2D(static_cast<float>(i - n + 1), static_cast<float>(values[static_cast<size_t>(i)])));
    }
    Q_EMIT gainWasteValueChanged();
}

void TLootAndWasteWidgetController::onGaugeTargetValueChanged(const QString& TargetValueString)
{
    bool ok = false;
    const qint64 value = gaugeDialogNumber(TargetValueString).toLongLong(&ok);
    if (ok)
        m_storage->setGaugeTarget(m_side, value);
}

void TLootAndWasteWidgetController::onWidgetOptionsChanged() { saveWidgetOptions(); }

void TLootAndWasteWidgetController::onCharacterBasedLootWasteOptionsChanged() { Q_EMIT characterBasedWidgetOptionsChanged(); }

QString TLootAndWasteWidgetController::gainWasteValueString() const { return format::multi(m_storage->sums(m_side).lastHour); }

QString TLootAndWasteWidgetController::gainWastePerHourString() const { return format::multi(static_cast<qint64>(gainWastePerHour())); }

double TLootAndWasteWidgetController::gainWastePerHour() const { return m_storage->sums(m_side).perHour; }

QVariantList TLootAndWasteWidgetController::gainWasteHistory() const { return m_history; }

double TLootAndWasteWidgetController::gainWasteGraphMaxY() const { return m_maxY; }

QString TLootAndWasteWidgetController::translationPrefix() const
{
    return m_side == TGainWasteStorage::Loot ? QStringLiteral("loot_") : QStringLiteral("waste_");
}

QString TLootAndWasteWidgetController::imagePrefix() const
{
    return m_side == TGainWasteStorage::Loot ? QStringLiteral("loot") : QStringLiteral("waste");
}

bool TLootAndWasteWidgetController::isGaugeVisible() const { return m_storage->gaugeVisible(m_side) && isPremiumPlayer(); }

bool TLootAndWasteWidgetController::isGraphVisible() const { return m_storage->graphVisible(m_side) && isPremiumPlayer(); }

double TLootAndWasteWidgetController::gaugeTargetValue() const { return static_cast<double>(m_storage->gaugeTarget(m_side)); }

QString TLootAndWasteWidgetController::gaugeTargetValueString() const { return format::multi(m_storage->gaugeTarget(m_side)); }

bool TLootAndWasteWidgetController::isGaugeMirrored() const { return m_side == TGainWasteStorage::Waste; }

QObject* TLootAndWasteWidgetController::displayedItems() const { return m_model; }

void TLootAndWasteWidgetController::requestConfigurationContextMenu()
{
    auto* menu = gamewindow::TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    connect(menu->addCustomEntry(qtTrId(qPrintable(translationPrefix() + QStringLiteral("reset")))), &gamewindow::TContextMenuEntryItem::triggered,
            this, [this] {
                m_maxY = 1.0;
                m_storage->reset(m_side);
            });
    menu->addCustomSeparator();
    addGaugeEntries();
    addGraphEntry();
    menu->showCustomMenu();
}

void TLootAndWasteWidgetController::requestGaugeContextMenu()
{
    auto* menu = gamewindow::TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    addGaugeEntries();
    menu->showCustomMenu();
}

void TLootAndWasteWidgetController::requestGraphContextMenu()
{
    auto* menu = gamewindow::TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    addGraphEntry();
    menu->showCustomMenu();
}

void TLootAndWasteWidgetController::addGaugeEntries()
{
    auto* menu = gamewindow::TContextMenuController::instance();
    const bool premium = isPremiumPlayer();
    const QString prefix = translationPrefix();
    if (premium) {
        connect(menu->addCustomEntry(qtTrId(qPrintable(prefix + QStringLiteral("set_gauge_target")))), &gamewindow::TContextMenuEntryItem::triggered,
                this, &TLootAndWasteWidgetController::showTargetDialog);
    }
    gamewindow::TContextMenuEntryItem* gauge =
        menu->addCustomEntry(qtTrId(qPrintable(prefix + QStringLiteral("show_gauge"))), premium ? QString() : qtTrId("context_premium_feature"));
    gauge->setCheckable(true);
    gauge->setChecked(m_storage->gaugeVisible(m_side) && premium);
    connect(gauge, &gamewindow::TContextMenuEntryItem::toggled, this, [this](const bool checked) { m_storage->setGaugeVisible(m_side, checked); });
}

void TLootAndWasteWidgetController::addGraphEntry()
{
    auto* menu = gamewindow::TContextMenuController::instance();
    const bool premium = isPremiumPlayer();
    gamewindow::TContextMenuEntryItem* graph = menu->addCustomEntry(qtTrId(qPrintable(translationPrefix() + QStringLiteral("show_graph"))),
                                                                    premium ? QString() : qtTrId("context_premium_feature"));
    graph->setCheckable(true);
    graph->setChecked(m_storage->graphVisible(m_side) && premium);
    connect(graph, &gamewindow::TContextMenuEntryItem::toggled, this, [this](const bool checked) { m_storage->setGraphVisible(m_side, checked); });
}

void TLootAndWasteWidgetController::showTargetDialog()
{
    if (!m_targetDialog) {
        const QString prefix = translationPrefix();
        m_targetDialog = new gamewindow::TGenericEnterTextDialogController();
        m_targetDialog->setCaption(qtTrId(qPrintable(prefix + QStringLiteral("gaugetarget_caption"))));
        m_targetDialog->setDescription(qtTrId(qPrintable(prefix + QStringLiteral("gaugetarget_description"))));
        m_targetDialog->setMaxInputLength(9);
        connect(m_targetDialog, &gamewindow::TGenericEnterTextDialogController::textEntered, this,
                &TLootAndWasteWidgetController::onGaugeTargetValueChanged);
    }
    m_targetDialog->setText(QString::number(m_storage->gaugeTarget(m_side)));
    m_targetDialog->dialog()->show();
}

// ----------------------------------------------------------------------------------------------
// TAnalyserWidgets

namespace {

/// SAVE_TEXT_TO_FILE / SAVE_JSON_TO_FILE: `text` into `filePath`, making its folder.
bool saveToFile(const input::TGameAction& action)
{
    if (action.filePath.isEmpty())
        return true;
    QDir().mkpath(QFileInfo(action.filePath).absolutePath());
    QFile file(action.filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(action.text.toUtf8());
    return true;
}

}

TAnalyserWidgets::TAnalyserWidgets(TAnalyserStorages* storages, gamewindow::TSidebarWidgetsManager* widgets, QQuickItem* gameWindowItem,
                                   QQmlEngine* engine, std::function<config::TSidebarOptions*()> options, QObject* parent)
    : QObject(parent)
    , m_storages(storages)
    , m_widgets(widgets)
    , m_gameWindowItem(gameWindowItem)
    , m_engine(engine)
    , m_options(std::move(options))
{
    add<TAnalyticsSelectorWidgetController>(W::AnalyticsSelector, [this] {
        auto* controller = new TAnalyticsSelectorWidgetController(m_widgets, m_gameWindowItem, m_engine);
        controller->setSidebarOptions(m_options());
        return controller;
    });
    add<THuntingSessionWidgetController>(W::HuntingSessionAnalyser, [this]() -> THuntingSessionWidgetController* {
        if (!isPremiumPlayer()) {
            publishAction(A::SET_STATUS_LINE, qtTrId("battlelist_need_premium"));
            return nullptr;
        }
        auto* controller = new THuntingSessionWidgetController(m_storages, m_gameWindowItem, m_engine);
        controller->setSidebarOptions(m_options());
        return controller;
    });
    add<TProgressWidgetController>(W::XpAnalyser, [this] {
        auto* controller = new TProgressWidgetController(m_storages->progress(), m_gameWindowItem, m_engine);
        controller->setSidebarOptions(m_options());
        return controller;
    });
    add<TLootAndWasteWidgetController>(W::LootAnalyser, [this] {
        auto* controller = new TLootAndWasteWidgetController(m_storages->gainWaste(), TGainWasteStorage::Loot, m_gameWindowItem, m_engine);
        controller->setSidebarOptions(m_options());
        return controller;
    });
    add<TLootAndWasteWidgetController>(W::WasteAnalyser, [this] {
        auto* controller = new TLootAndWasteWidgetController(m_storages->gainWaste(), TGainWasteStorage::Waste, m_gameWindowItem, m_engine);
        controller->setSidebarOptions(m_options());
        return controller;
    });
    add<TImpactWidgetController>(W::ImpactAnalyser, [this] {
        auto* controller = new TImpactWidgetController(m_storages->impact(), m_gameWindowItem, m_engine);
        controller->setSidebarOptions(m_options());
        return controller;
    });
    add<TDamageInputWidgetController>(W::DamageInputAnalyser, [this] {
        auto* controller = new TDamageInputWidgetController(m_storages->damageInput(), m_gameWindowItem, m_engine);
        controller->setSidebarOptions(m_options());
        return controller;
    });
    add<TPartyHuntWidgetController>(W::PartyHuntAnalyser, [this] {
        auto* controller = new TPartyHuntWidgetController(m_storages->partyHunt(), m_gameWindowItem, m_engine);
        controller->setSidebarOptions(m_options());
        return controller;
    });

    input::setActionHandler(A::RESET_HUNTING_SESSION, [this](const input::TGameAction&) {
        m_storages->resetAll();
        return true;
    });
    input::setActionHandler(A::SAVE_TEXT_TO_FILE, saveToFile);
    input::setActionHandler(A::SAVE_JSON_TO_FILE, saveToFile);
}

TAnalyserWidgets::~TAnalyserWidgets()
{
    input::setActionHandler(A::RESET_HUNTING_SESSION, nullptr);
    input::setActionHandler(A::SAVE_TEXT_TO_FILE, nullptr);
    input::setActionHandler(A::SAVE_JSON_TO_FILE, nullptr);
    for (auto& [type, slot] : m_slots)
        delete slot.controller;
}

template <class Controller>
void TAnalyserWidgets::add(const int widgetType, std::function<Controller*()> make)
{
    Slot& slot = m_slots[widgetType];
    slot.make = std::move(make);
    slot.widgetOf = [](QObject* controller) { return static_cast<Controller*>(controller)->widget(); };
    if (!m_widgets)
        return;
    m_widgets->registerWidget(widgetType, {
                                              [this, widgetType]() -> gamewindow::TSidebarWidget* {
                                                  Slot& s = m_slots[widgetType];
                                                  if (!s.controller) {
                                                      s.controller = s.make();
                                                      if (!s.controller)
                                                          return nullptr;
                                                  }
                                                  return s.widgetOf(s.controller);
                                              },
                                              [this, widgetType] {
                                                  Slot& s = m_slots[widgetType];
                                                  if (!s.controller)
                                                      return;
                                                  QMetaObject::invokeMethod(s.controller, "saveWidgetOptions");
                                                  s.controller->deleteLater();
                                                  s.controller = nullptr;
                                              },
                                              [this, widgetType]() -> gamewindow::TSidebarWidget* {
                                                  Slot& s = m_slots[widgetType];
                                                  return s.controller ? s.widgetOf(s.controller) : nullptr;
                                              },
                                          });
}

}
