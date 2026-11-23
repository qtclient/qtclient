#include "skillswidgetcontroller.h"

#include "analysers/analyserstorages.h"
#include "config/characterconfigurationmanager.h"
#include "creatures/creaturestorage.h"
#include "cyclopedia/combatstatsqmldata.h"
#include "game/gameciptypes.h"
#include "game/gamesessionbase.h"
#include "game/playerdata.h"
#include "game/playerskillstats.h"
#include "gamewindow/contextmenucontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <QQuickItem>

#include <iterator>

namespace tibia::gamewindow {

namespace {

using game::SkillType;
using Separator = game::SkillsWidgetEntry::Separator;

/// The widget's rows, top to bottom: (type, separator).
constexpr std::pair<SkillType, Separator> Entries[] = {
    { SkillType::Level, Separator::NONE },
    { SkillType::Experience, Separator::NONE },
    { SkillType::ExperienceGain, Separator::NONE },
    { SkillType::ExperienceGain, Separator::XP_BOOST_BUTTON },
    { SkillType::HitPoints, Separator::NONE },
    { SkillType::Mana, Separator::NONE },
    { SkillType::SoulPoints, Separator::NONE },
    { SkillType::Capacity, Separator::NONE },
    { SkillType::Speed, Separator::NONE },
    { SkillType::Food, Separator::NONE },
    { SkillType::Stamina, Separator::NONE },
    { SkillType::OfflineTraining, Separator::NONE },
    { static_cast<SkillType>(0), Separator::SEPARATOR },
    { SkillType::MagicLevel, Separator::NONE },
    { SkillType::Fist, Separator::NONE },
    { SkillType::Club, Separator::NONE },
    { SkillType::Sword, Separator::NONE },
    { SkillType::Axe, Separator::NONE },
    { SkillType::Distance, Separator::NONE },
    { SkillType::Shielding, Separator::NONE },
    { SkillType::Fishing, Separator::NONE },
};

bool isHidden(const config::TSkillsWidgetOptions& options, const SkillType type)
{
    return options.invisibleProgressBars.count(static_cast<int>(type)) != 0;
}

}

TSkillsWidgetController::TSkillsWidgetController(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
    , m_entries(std::begin(Entries), std::end(Entries))
{
    widget()->createItem(QStringLiteral("skillswidget.qml"));
    createEntries();
    if (QQuickItem* item = quickItem()) {
        m_connections.add(connect(item, SIGNAL(requestContextMenu()), this, SLOT(onRequestContextMenu())));
        m_connections.add(connect(item, SIGNAL(beforeTooltipShow()), this, SLOT(onBeforeTooltipShow())));
        m_connections.add(connect(item, SIGNAL(openStoreWithXpBoost()), this, SLOT(onStoreXpBoostButtonClicked())));
        m_connections.add(connect(item, SIGNAL(openMoreStatDetails()), this, SLOT(onMoreStatDetailsClicked())));
    }
}

TSkillsWidgetController::~TSkillsWidgetController() = default;

QList<QObject*> TSkillsWidgetController::skillsModel() const { return m_skillsModel; }
QObject* TSkillsWidgetController::offenceStats() const { return m_offenceStats; }
QObject* TSkillsWidgetController::defenceStats() const { return m_defenceStats; }
QObject* TSkillsWidgetController::miscStats() const { return m_miscStats; }
bool TSkillsWidgetController::showOffenceStats() const { return m_widgetOptions.offenceStatsVisible; }
bool TSkillsWidgetController::showDefenceStats() const { return m_widgetOptions.defenceStatsVisible; }
bool TSkillsWidgetController::showMiscStats() const { return m_widgetOptions.miscStatsVisible; }

QQuickItem* TSkillsWidgetController::gameWindowQuickItem() const { return widget()->gameWindowItem(); }

void TSkillsWidgetController::onCloseClicked() { onRequestClose(); }
void TSkillsWidgetController::onRequestClose() { Q_EMIT widgetClosed(); }
void TSkillsWidgetController::onRequestMoveToSidebar(const int TargetSidebar)
{
    widget()->requestMoveToSidebar(TargetSidebar);
    Q_EMIT widgetWantsToMoveToSidebar(TargetSidebar, widget());
}
void TSkillsWidgetController::onSetDragActive(const bool Active)
{
    widget()->setDragActive(Active);
    Q_EMIT widgetDragChanged(parentSidebarId(), Active);
}
void TSkillsWidgetController::onSidebarDestroyed() { widget()->onSidebarDestroyed(); }
void TSkillsWidgetController::onLevelUpClicked() {}
void TSkillsWidgetController::setCursorToResizeVertical(const bool Enabled)
{
    widget()->setCursorToResizeVertical(Enabled);
}
void TSkillsWidgetController::startContentSizeChangedTimer() { widget()->startContentSizeChangedTimer(); }

void TSkillsWidgetController::createEntries()
{
    m_skillsModel.clear();
    m_skillsModel.reserve(static_cast<qsizetype>(m_entries.size()));
    for (const auto& [type, separator] : m_entries) {
        auto* data = new game::TSkillsWidgetEntryData(this);
        m_skillsModel.append(data);
        game::SkillsWidgetEntry initial;
        initial.separatorType = separator;
        if (separator == Separator::NONE) {
            initial.progressBarVisible = !isHidden(m_widgetOptions, type);
            data->assign(initial);
            m_entriesByType[type] = data;
        } else {
            data->assign(initial);
            m_separatorEntries.emplace_back(type, data);
        }
    }
    Q_EMIT skillsModelChanged();
}

game::TSkillsWidgetEntryData* TSkillsWidgetController::entry(const SkillType type) const
{
    const auto it = m_entriesByType.find(type);
    return it != m_entriesByType.end() ? it->second : nullptr;
}

void TSkillsWidgetController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    m_gameSessionConnections.disconnectAll();
    m_options = nullptr;
    m_skillStats = nullptr;
    m_gameSession = GameSession;
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    if (session) {
        if (config::TCharacterConfigurationManager* configuration = session->characterConfigurationManager().get())
            m_options = configuration->sidebarOptions();
        if (game::TPlayerData* playerData = session->playerData().get())
            m_gameSessionConnections.add(
                connect(playerData, &game::TPlayerData::playerDataChanged, this, &TSkillsWidgetController::onPlayerDataChanged));
        m_skillStats = session->playerSkillStats();
        if (m_skillStats)
            m_gameSessionConnections.add(connect(m_skillStats.get(), &game::TPlayerSkillStats::playerSkillStatsChanged, this,
                                                 &TSkillsWidgetController::onPlayerSkillStatsChanged));
        if (m_options) {
            m_gameSessionConnections.add(connect(m_options.data(), &config::TSidebarOptions::skillsWidgetOptionsChanged, this,
                                                 &TSkillsWidgetController::onWidgetOptionsChanged));
            onWidgetOptionsChanged();
        }
    }
    onPlayerSkillStatsChanged();
    onPlayerDataChanged();
}

void TSkillsWidgetController::setPremiumTimeStorage(game::TPremiumTimeStorage* premiumTimeStorage)
{
    if (m_premiumTimeStorage)
        disconnect(m_premiumTimeStorage, &game::TPremiumTimeStorage::premiumTimeChanged, this,
                   &TSkillsWidgetController::onPlayerDataChanged);
    m_premiumTimeStorage = premiumTimeStorage;
    if (!m_premiumTimeStorage)
        return;
    connect(m_premiumTimeStorage, &game::TPremiumTimeStorage::premiumTimeChanged, this, &TSkillsWidgetController::onPlayerDataChanged);
    onPlayerDataChanged();
}

void TSkillsWidgetController::saveWidgetOptions()
{
    if (!m_options)
        return;
    m_widgetOptions.setContentHeightAndMaximized(widget()->maximizedContentHeight(), widget()->isMaximized());
    m_options->setSkillsWidgetOptions(m_widgetOptions);
}

void TSkillsWidgetController::onWidgetOptionsChanged()
{
    if (!m_options)
        return;
    m_widgetOptions = m_options->skillsWidgetOptions();
    widget()->setInitialContentHeightAndMaximized(m_widgetOptions.contentHeight, m_widgetOptions.contentMaximized);
    applyProgressBarVisibilityFromOptions();
}

void TSkillsWidgetController::applyProgressBarVisibilityFromOptions()
{
    const std::set<int> hidden = m_widgetOptions.invisibleProgressBars;
    for (const auto& [type, separator] : m_entries) {
        if (separator == Separator::NONE && game::skillHasProgressBar(type))
            setProgressBarVisible(type, hidden.count(static_cast<int>(type)) == 0);
    }
}

void TSkillsWidgetController::setProgressBarVisible(const SkillType type, const bool visible)
{
    std::set<int> hidden = m_widgetOptions.invisibleProgressBars;
    if (visible)
        hidden.erase(static_cast<int>(type));
    else
        hidden.insert(static_cast<int>(type));
    const bool changed = hidden != m_widgetOptions.invisibleProgressBars;
    m_widgetOptions.invisibleProgressBars = hidden;
    // The entry's bar follows the options' set (the entry is rebuilt with it).
    if (entry(type))
        refreshEntry(type);
    if (changed)
        saveWidgetOptions();
}

void TSkillsWidgetController::refreshEntry(const SkillType type)
{
    game::TSkillsWidgetEntryData* data = entry(type);
    if (!data)
        return;
    game::SkillsWidgetEntry built = game::buildSkillsWidgetEntry(type);
    built.progressBarVisible = built.progressBarVisible && !isHidden(m_widgetOptions, type);
    built.separatorType = Separator::NONE;
    data->assign(built);
}

void TSkillsWidgetController::onBeforeTooltipShow() { onPlayerDataChanged(); }

void TSkillsWidgetController::onPlayerDataChanged()
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    if (!session)
        return;
    game::TPlayerData* playerData = session->playerData().get();
    if (!playerData)
        return;
    if (const std::shared_ptr<creatures::TCreature> player = session->playerCreature().lock())
        playerData->setSpeed(player->speed());
    for (const auto& [type, data] : m_entriesByType)
        refreshEntry(type);
    for (const auto& [type, data] : m_separatorEntries) {
        game::SkillsWidgetEntry separator;
        separator.separatorType = data->separatorType();
        separator.tooltip = QString();
        separator.showButton = type == SkillType::ExperienceGain && playerData->storeXpBoostPurchasable();
        data->assign(separator);
    }
}

void TSkillsWidgetController::onPlayerSkillStatsChanged()
{
    // Without a session the blocks show an empty PlayerSkills (all zero).
    const std::shared_ptr<game::TPlayerSkillStats> stats =
        m_skillStats ? m_skillStats : std::make_shared<game::TPlayerSkillStats>();
    QObject* const previous[] = { m_offenceStats, m_defenceStats, m_miscStats };
    m_offenceStats = new cyclopedia::TOffenceStatsQmlData(stats, this);
    m_defenceStats = new cyclopedia::TDefenceStatsQmlData(stats, this);
    m_miscStats = new cyclopedia::TMiscStatsQmlData(stats, this);
    Q_EMIT combatStatsChanged();
    for (QObject* object : previous) {
        if (object)
            object->deleteLater();
    }
}

void TSkillsWidgetController::onStoreXpBoostButtonClicked()
{
    auto action = std::make_shared<input::TGameActionOpenStore>(1, 1);
    Q_EMIT publishGameAction(action);
}

void TSkillsWidgetController::onMoreStatDetailsClicked()
{
    auto action = std::make_shared<input::TGameActionOpenCyclopedia>(2);
    action->setCategory(1);
    Q_EMIT publishGameAction(action);
}

void TSkillsWidgetController::onRequestContextMenu()
{
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    // Resets the experience counter: the character configuration's progress
    // storage.
    connect(menu->addCustomEntry(qtTrId("rest_exp_counter")), &TContextMenuEntryItem::triggered, this, [this] {
        const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
        if (!session)
            return;
        if (config::TCharacterConfigurationManager* configuration = session->characterConfigurationManager().get()) {
            if (analysers::TProgressStorage* progress = configuration->progressStorage())
                progress->reset();
        }
    });
    menu->addCustomSeparator();

    bool allVisible = true;
    for (const auto& [type, separator] : m_entries) {
        if (separator != Separator::NONE || !game::skillHasProgressBar(type))
            continue;
        game::TSkillsWidgetEntryData* data = entry(type);
        const bool visible = data && data->progressBarVisible();
        allVisible &= visible;
        TContextMenuEntryItem* item = menu->addCustomEntry(game::skillName(type).section(QChar(0x9c), 0, 0));
        item->setCheckable(true);
        item->setChecked(visible);
        connect(item, &TContextMenuEntryItem::toggled, this,
                [this, type = type](const bool checked) { setProgressBarVisible(type, checked); });
    }
    menu->addCustomSeparator();

    // The stat blocks: the options copy takes the box's state, is saved and
    // the widget re-reads showOffenceStats / showDefenceStats / showMiscStats.
    const auto addStats = [&](const char* label, bool config::TSkillsWidgetOptions::*member) {
        TContextMenuEntryItem* item = menu->addCustomEntry(qtTrId(label));
        item->setCheckable(true);
        item->setChecked(m_widgetOptions.*member);
        connect(item, &TContextMenuEntryItem::toggled, this, [this, member](const bool checked) {
            m_widgetOptions.*member = checked;
            saveWidgetOptions();
            Q_EMIT widgetOptionsChanged();
        });
    };
    addStats("skills_show_offence_stats", &config::TSkillsWidgetOptions::offenceStatsVisible);
    addStats("skills_show_defence_stats", &config::TSkillsWidgetOptions::defenceStatsVisible);
    addStats("skills_show_misc_stats", &config::TSkillsWidgetOptions::miscStatsVisible);
    menu->addCustomSeparator();

    TContextMenuEntryItem* all = menu->addCustomEntry(qtTrId("show_all_skill_bars"));
    all->setCheckable(true);
    all->setChecked(allVisible);
    connect(all, &TContextMenuEntryItem::toggled, this, [this](const bool checked) {
        for (const auto& [type, separator] : m_entries) {
            if (separator == Separator::NONE && game::skillHasProgressBar(type))
                setProgressBarVisible(type, checked);
        }
    });
    menu->showCustomMenu();
}

}
