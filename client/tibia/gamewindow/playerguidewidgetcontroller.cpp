#include "playerguidewidgetcontroller.h"

#include "config/sidebaroptions.h"
#include "game/playerdata.h"
#include "game/tgamenotify.h"
#include "gamewindow/remainingdialogs.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "news/newsstorage.h"
#include "spells/spellinfostorage.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QQuickItem>

#include <algorithm>
#include <tibia/game/gamesession.h>

namespace tibia::gamewindow {

namespace {

constexpr auto OptionsKey = "playerGuideWidgetOptions";
constexpr auto PendingPopupsKey = "pendingLevelSpecialPopups";
constexpr auto ConfirmedPopupsKey = "confirmedLevelSpecialPopups";
/// How long a level up stays on the widget.
constexpr int LevelUpShowMilliseconds = 15000;
/// The player guide shows at most this many unlocked entries.
constexpr size_t MaxPlayerGuideEntries = 3;

/// The levels with a special popup.
const std::set<int>& levelSpecialPopupLevels()
{
    static const std::set<int> levels = { 9, 10, 20 };
    return levels;
}

/// One page of a special popup: the backdrop and its tab icon.
struct TPopupPage
{
    QString image;
    QString icon;
};

/// A level's special popup.
struct TLevelSpecialPopup
{
    bool showDragonHeader = false;
    bool showCrestHeader = true;
    QString caption;
    QString banner;
    std::vector<TPopupPage> pages;
};

TLevelSpecialPopup levelSpecialPopup(const int level)
{
    TLevelSpecialPopup popup;
    switch (level) {
    case 9:
        popup.caption = qtTrId("playerguidewidget_level_9_special_popup_name");
        popup.banner = QStringLiteral("/images/tutorial/banner_tutorialpopup_level9.png");
        popup.pages = {
            { QStringLiteral("/images/tutorial/backdrop_popup_depot.png"), QStringLiteral("/images/icon-depot.png") },
            { QStringLiteral("/images/tutorial/backdrop_popup_supplystash.png"), QStringLiteral("/images/icon-supplystash.png") },
            { QStringLiteral("/images/tutorial/backdrop_popup_market.png"), QStringLiteral("/images/icon-marketchest.png") },
        };
        break;
    case 10:
        popup.caption = qtTrId("playerguidewidget_level_10_special_popup_name");
        popup.banner = QStringLiteral("/images/tutorial/banner_tutorialpopup_level10.png");
        popup.pages = { { QStringLiteral("/images/tutorial/backdrop_popup_level10.jpg"), QString() } };
        break;
    case 20:
        popup.caption = qtTrId("playerguidewidget_level_20_special_popup_name");
        popup.banner = QStringLiteral("/images/tutorial/banner_tutorialpopup_level20.png");
        popup.pages = { { QStringLiteral("/images/tutorial/backdrop_popup_level20.jpg"), QString() } };
        break;
    default: break;
    }
    return popup;
}

std::set<int> levelSet(const QJsonObject& json, const char* key)
{
    std::set<int> levels;
    for (const QJsonValue& level : json.value(QLatin1String(key)).toArray())
        levels.insert(level.toInt());
    return levels;
}

QJsonArray levelArray(const std::set<int>& levels)
{
    QJsonArray array;
    for (const int level : levels)
        array.append(level);
    return array;
}

input::IGameActionPtr openNewsEntryAction(const int newsEntryId)
{
    return input::TGameActionOpenNewsDialog::withNews(static_cast<uint32_t>(newsEntryId));
}

}

void TPlayerGuideSpellListModel::setEntries(std::vector<TEntry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
}

int TPlayerGuideSpellListModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : int(m_entries.size()); }

QVariant TPlayerGuideSpellListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= int(m_entries.size()))
        return {};
    const TEntry& entry = m_entries[size_t(index.row())];
    if (role == 0x101)
        return entry.name;
    if (role == 0x102)
        return QStringLiteral("image://spell-icons-32x32/%1").arg(entry.iconIndex);
    return qtTrId("dummy_unknown");
}

QHash<int, QByteArray> TPlayerGuideSpellListModel::roleNames() const { return { { 0x101, "spellName" }, { 0x102, "iconID" } }; }

void TPlayerGuideEntryListModel::setEntries(std::vector<TEntry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
}

int TPlayerGuideEntryListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(m_entries.size());
}

QVariant TPlayerGuideEntryListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= int(m_entries.size()))
        return {};
    const TEntry& entry = m_entries[size_t(index.row())];
    if (role == 0x102)
        return uint(entry.newsEntryId);
    if (role == 0x101)
        return entry.header;
    return qtTrId("dummy_unknown");
}

QHash<int, QByteArray> TPlayerGuideEntryListModel::roleNames() const
{
    return { { 0x101, "playerGuideHeader" }, { 0x102, "newsEntryID" } };
}

TPlayerGuideWidgetController::TPlayerGuideWidgetController(news::TNewsStorage* newsStorage, QQuickItem* gameWindowItem,
                                                           QQmlEngine* engine, QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
    , m_newsStorage(newsStorage)
{
    m_autoSpellList = &m_autoSpells;
    m_manualSpellList = &m_manualSpells;
    m_previousUnlockedPlayerGuideEntries = &m_previousEntries;
    m_hideTimer.setInterval(LevelUpShowMilliseconds);
    m_hideTimer.setSingleShot(true);
    connect(&m_hideTimer, &QTimer::timeout, this, &TPlayerGuideWidgetController::hideLevelUpUnlessTimerRuns);
    widget()->createItem(QStringLiteral("PlayerGuideWidget.qml"));
    if (m_newsStorage)
        connect(m_newsStorage, &news::TNewsStorage::contentsChanged, this, &TPlayerGuideWidgetController::onNewsStorageContentsChanged);
    connect(game::TGameNotify::instance(), &game::TGameNotify::gameStarted, this, &TPlayerGuideWidgetController::onGameSessionChanged);
    connect(game::TGameNotify::instance(), &game::TGameNotify::gameSessionChanged, this, &TPlayerGuideWidgetController::onGameSessionChanged);
    if (tibia::game::isOnline())
        onGameSessionChanged();
}

void TPlayerGuideWidgetController::onGameSessionChanged()
{
    if (game::TPlayerData* player = game::TPlayerData::liveInstance()) {
        connect(player, &game::TPlayerData::playerLevelUp, this, &TPlayerGuideWidgetController::onPlayerLevelUp, Qt::UniqueConnection);
        onWidgetOptionsChanged();
        refreshPlayerGuideEntries();
    }
    hideLevelUpUnlessTimerRuns();
}

void TPlayerGuideWidgetController::setFlag(bool& flag, const bool value, void (TPlayerGuideWidgetController::*changed)())
{
    if (flag == value)
        return;
    flag = value;
    Q_EMIT(this->*changed)();
}

void TPlayerGuideWidgetController::onPlayerLevelUp()
{
    m_hideTimer.start();
    setFlag(m_isShowLevelUp, true, &TPlayerGuideWidgetController::showLevelUpChanged);
    const game::TPlayerData* player = game::TPlayerData::liveInstance();
    const int level = player ? int(player->level()) : 0;
    if (m_playerLevelToShow != level) {
        m_playerLevelToShow = level;
        Q_EMIT playerLevelToShowChanged();
    }
    // The spells of exactly this level for the player's vocation: unlocked
    // ones apart from the ones to learn.
    std::vector<TPlayerGuideSpellListModel::TEntry> autoSpells;
    std::vector<TPlayerGuideSpellListModel::TEntry> manualSpells;
    const uint8_t vocation = player ? player->vocation() : 0;
    const spells::TSpellInfoStorage* storage = spells::TSpellInfoStorage::instance();
    for (const auto& [id, record] : storage->spells()) {
        const spells::TSpellInfo spell = storage->spellInfoAt(id);
        if (!spell.cast.vocations.count(vocation) || int(spell.cast.minimumLevel) != m_playerLevelToShow)
            continue;
        (spell.autoUnlock ? autoSpells : manualSpells).push_back({ spell.iconIndex, spell.name });
    }
    const bool hasAutoSpells = !autoSpells.empty();
    const bool hasManualSpells = !manualSpells.empty();
    m_autoSpells.setEntries(std::move(autoSpells));
    m_manualSpells.setEntries(std::move(manualSpells));
    setFlag(m_isShowAutoSpellList, hasAutoSpells, &TPlayerGuideWidgetController::showAutoSpellListChanged);
    setFlag(m_isShowManualSpellList, hasManualSpells, &TPlayerGuideWidgetController::showManualSpellListChanged);
    refreshPlayerGuideEntries();
}

void TPlayerGuideWidgetController::hideLevelUpUnlessTimerRuns()
{
    if (!m_hideTimer.isActive()) {
        setFlag(m_isShowLevelUp, false, &TPlayerGuideWidgetController::showLevelUpChanged);
        setFlag(m_isShowAutoSpellList, false, &TPlayerGuideWidgetController::showAutoSpellListChanged);
        setFlag(m_isShowManualSpellList, false, &TPlayerGuideWidgetController::showManualSpellListChanged);
    }
    Q_EMIT playerGuideEntriesChanged();
    Q_EMIT levelSpecialPopupLevelChanged();
}

void TPlayerGuideWidgetController::refreshPlayerGuideEntries()
{
    // Unlocked player guide news the character's level reached, highest
    // unlock level first, then by headline.
    std::vector<const news::TNewsEntry*> items;
    if (m_newsStorage) {
        for (const auto& [id, item] : m_newsStorage->entries()) {
            if (item.isUnlocked() && item.unlockLevel() <= m_newsStorage->characterLevel()
                && item.type() != news::NEWS_TYPE_GROUP_HEADER && item.category() == news::NEWS_CATEGORY_PLAYER_GUIDE)
                items.push_back(&item);
        }
    }
    std::sort(items.begin(), items.end(), [](const news::TNewsEntry* a, const news::TNewsEntry* b) {
        if (a->unlockLevel() != b->unlockLevel())
            return b->unlockLevel() < a->unlockLevel();
        return QString::compare(a->headline(), b->headline(), Qt::CaseSensitive) < 0;
    });
    if (items.size() > MaxPlayerGuideEntries)
        items.resize(MaxPlayerGuideEntries);
    std::vector<TPlayerGuideEntryListModel::TEntry> previous;
    if (items.empty()) {
        m_latestUnlockedPlayerGuideEntry = QString();
        m_latestUnlockedNewsEntryId = 0;
        setFlag(m_hasLatestUnlockedPlayerGuideEntry, false, &TPlayerGuideWidgetController::playerGuideEntriesChanged);
    } else {
        m_latestUnlockedPlayerGuideEntry = items.front()->headline();
        m_latestUnlockedNewsEntryId = static_cast<int>(items.front()->id());
        for (auto it = items.begin() + 1; it != items.end(); ++it)
            previous.push_back({ (*it)->headline(), static_cast<int>((*it)->id()) });
        setFlag(m_hasLatestUnlockedPlayerGuideEntry, true, &TPlayerGuideWidgetController::playerGuideEntriesChanged);
    }
    const bool hasPrevious = !previous.empty();
    m_previousEntries.setEntries(std::move(previous));
    setFlag(m_hasPreviousUnlockedPlayerGuideEntries, hasPrevious, &TPlayerGuideWidgetController::playerGuideEntriesChanged);
    hideLevelUpUnlessTimerRuns();
}

std::optional<int> TPlayerGuideWidgetController::levelSpecialPopupLevel() const
{
    const game::TPlayerData* player = tibia::game::isOnline() ? game::TPlayerData::liveInstance() : nullptr;
    if (!player || !m_options)
        return std::nullopt;
    const std::set<int> pending = levelSet(m_options->section(QLatin1String(OptionsKey)).toObject(), PendingPopupsKey);
    const int level = pending.empty() ? 0 : *pending.begin();
    if (!levelSpecialPopupLevels().count(level) || int(player->level()) < level)
        return std::nullopt;
    return level;
}

void TPlayerGuideWidgetController::queueLevelSpecialPopup(const int level, config::TSidebarOptions* options)
{
    if (!levelSpecialPopupLevels().count(level) || !options)
        return;
    QJsonObject json = options->section(QLatin1String(OptionsKey)).toObject();
    if (levelSet(json, ConfirmedPopupsKey).count(level))
        return;
    std::set<int> pending = levelSet(json, PendingPopupsKey);
    if (!pending.insert(level).second)
        return;
    json.insert(QLatin1String(PendingPopupsKey), levelArray(pending));
    options->setSection(QLatin1String(OptionsKey), json);
}

bool TPlayerGuideWidgetController::isShowLevelUp() const { return m_isShowLevelUp; }
bool TPlayerGuideWidgetController::isShowAutoSpellList() const { return m_isShowAutoSpellList; }
bool TPlayerGuideWidgetController::isShowManualSpellList() const { return m_isShowManualSpellList; }
int TPlayerGuideWidgetController::playerLevelToShow() const { return m_playerLevelToShow; }
QObject* TPlayerGuideWidgetController::autoSpellList() const { return m_autoSpellList; }
QObject* TPlayerGuideWidgetController::manualSpellList() const { return m_manualSpellList; }
bool TPlayerGuideWidgetController::isShowPlayerGuideEntries() const { return !m_isShowLevelUp && !levelSpecialPopupLevel(); }
bool TPlayerGuideWidgetController::hasLatestUnlockedPlayerGuideEntry() const { return m_hasLatestUnlockedPlayerGuideEntry; }
QString TPlayerGuideWidgetController::latestUnlockedPlayerGuideEntry() const { return m_latestUnlockedPlayerGuideEntry; }
bool TPlayerGuideWidgetController::hasPreviousUnlockedPlayerGuideEntries() const { return m_hasPreviousUnlockedPlayerGuideEntries; }
QObject* TPlayerGuideWidgetController::previousUnlockedPlayerGuideEntries() const { return m_previousUnlockedPlayerGuideEntries; }
bool TPlayerGuideWidgetController::isShowLevelSpecialPopup() const { return !m_isShowLevelUp && levelSpecialPopupLevel(); }

QString TPlayerGuideWidgetController::levelSpecialPopupName() const
{
    const std::optional<int> level = levelSpecialPopupLevel();
    return level ? levelSpecialPopup(*level).caption : QString();
}

QQuickItem* TPlayerGuideWidgetController::gameWindowQuickItem() const { return widget()->gameWindowItem(); }

void TPlayerGuideWidgetController::onCloseClicked() { onRequestClose(); }
void TPlayerGuideWidgetController::onRequestClose() { Q_EMIT widgetClosed(); }
void TPlayerGuideWidgetController::onRequestMoveToSidebar(const int TargetSidebar)
{
    widget()->requestMoveToSidebar(TargetSidebar);
    Q_EMIT widgetWantsToMoveToSidebar(TargetSidebar, widget());
}
void TPlayerGuideWidgetController::onSetDragActive(const bool Active)
{
    widget()->setDragActive(Active);
    Q_EMIT widgetDragChanged(parentSidebarId(), Active);
}
void TPlayerGuideWidgetController::onSidebarDestroyed() { widget()->onSidebarDestroyed(); }
void TPlayerGuideWidgetController::onLevelUpClicked() {}
void TPlayerGuideWidgetController::setCursorToResizeVertical(const bool Enabled) { widget()->setCursorToResizeVertical(Enabled); }
void TPlayerGuideWidgetController::startContentSizeChangedTimer() { widget()->startContentSizeChangedTimer(); }

void TPlayerGuideWidgetController::showLatestUnlockedPlayerGuide()
{
    if (m_latestUnlockedNewsEntryId != 0)
        Q_EMIT publishGameAction(openNewsEntryAction(m_latestUnlockedNewsEntryId));
}

void TPlayerGuideWidgetController::showPreviousUnlockedPlayerGuide(const int newsEntryId) { Q_EMIT publishGameAction(openNewsEntryAction(newsEntryId)); }

void TPlayerGuideWidgetController::showLevelSpecialPopup()
{
    const int level = levelSpecialPopupLevel().value_or(0);
    if (level <= 0)
        return;
    const TLevelSpecialPopup popup = levelSpecialPopup(level);
    if (TShowImageDialogController* images = TShowImageDialogController::instance()) {
        QStringList pages;
        for (const TPopupPage& page : popup.pages)
            pages << page.image;
        images->showImages(pages, popup.caption, popup.showDragonHeader, popup.showCrestHeader);
    }
    // The popup moves from pending to seen.
    if (m_options) {
        QJsonObject json = m_options->section(QLatin1String(OptionsKey)).toObject();
        std::set<int> pending = levelSet(json, PendingPopupsKey);
        std::set<int> confirmed = levelSet(json, ConfirmedPopupsKey);
        pending.erase(level);
        confirmed.insert(level);
        json.insert(QLatin1String(PendingPopupsKey), levelArray(pending));
        json.insert(QLatin1String(ConfirmedPopupsKey), levelArray(confirmed));
        m_options->setSection(QLatin1String(OptionsKey), json);
    }
    hideLevelUpUnlessTimerRuns();
}

void TPlayerGuideWidgetController::setSidebarOptions(config::TSidebarOptions* options)
{
    m_options = options;
    onWidgetOptionsChanged();
    Q_EMIT playerGuideEntriesChanged();
    Q_EMIT levelSpecialPopupLevelChanged();
}

void TPlayerGuideWidgetController::saveWidgetOptions()
{
    if (!m_options)
        return;
    QJsonObject json = m_options->section(QLatin1String(OptionsKey)).toObject();
    json.insert(QStringLiteral("contentHeight"), widget()->maximizedContentHeight());
    json.insert(QStringLiteral("contentMaximized"), widget()->isMaximized());
    m_options->setSection(QLatin1String(OptionsKey), json);
}

void TPlayerGuideWidgetController::onWidgetOptionsChanged()
{
    if (!m_options)
        return;
    const QJsonObject json = m_options->section(QLatin1String(OptionsKey)).toObject();
    if (json.contains(QLatin1String("contentHeight")))
        widget()->setInitialContentHeightAndMaximized(json.value(QLatin1String("contentHeight")).toInt(),
                                                       json.value(QLatin1String("contentMaximized")).toBool(true));
}

void TPlayerGuideWidgetController::onWidgetQuickItemDestroyed() {}

void TPlayerGuideWidgetController::onNewsStorageContentsChanged()
{
    refreshPlayerGuideEntries();
    Q_EMIT levelSpecialPopupLevelChanged();
}

TPlayerGuideWidgetManager::TPlayerGuideWidgetManager(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent)
    : QObject(parent)
    , m_gameWindowItem(gameWindowItem)
    , m_engine(engine)
{
    connect(game::TGameNotify::instance(), &game::TGameNotify::gameStarted, this, &TPlayerGuideWidgetManager::onGameStarted);
    connect(game::TGameNotify::instance(), &game::TGameNotify::gameSessionChanged, this, &TPlayerGuideWidgetManager::onGameStarted);
}

TPlayerGuideWidgetManager::~TPlayerGuideWidgetManager() { delete m_controller; }

void TPlayerGuideWidgetManager::onGameStarted()
{
    game::TPlayerData* player = game::TPlayerData::liveInstance();
    if (!player)
        return;
    connect(player, &game::TPlayerData::playerLevelUp, this, &TPlayerGuideWidgetManager::onPlayerLevelUp, Qt::UniqueConnection);
    // The news entries unlock against the character's level.
    if (news::TNewsStorage* storage = news::TNewsStorage::current())
        storage->raiseCharacterLevel(static_cast<ushort>(player->level()));
}

void TPlayerGuideWidgetManager::onPlayerLevelUp()
{
    const game::TPlayerData* player = game::TPlayerData::liveInstance();
    if (!player)
        return;
    TPlayerGuideWidgetController::queueLevelSpecialPopup(int(player->level()), m_options);
    if (news::TNewsStorage* storage = news::TNewsStorage::current())
        storage->raiseCharacterLevel(static_cast<ushort>(player->level()));
}

TSidebarWidget* TPlayerGuideWidgetManager::open(config::TSidebarOptions* options)
{
    m_options = options;
    if (!m_controller) {
        m_controller = new TPlayerGuideWidgetController(news::TNewsStorage::current(), m_gameWindowItem, m_engine);
        connect(m_controller, &TPlayerGuideWidgetController::publishGameAction, this, &TPlayerGuideWidgetManager::publishGameAction);
        m_controller->setSidebarOptions(options);
    }
    return m_controller->widget();
}

void TPlayerGuideWidgetManager::close(const bool saveWidgetOptions)
{
    if (!m_controller)
        return;
    if (saveWidgetOptions)
        m_controller->saveWidgetOptions();
    m_controller->deleteLater();
    m_controller = nullptr;
}

TSidebarWidget* TPlayerGuideWidgetManager::current() const { return m_controller ? m_controller->widget() : nullptr; }

}
