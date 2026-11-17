#include "cyclopediadialogcontroller.h"

#include "cyclopedia/bestiarystorage.h"
#include "cyclopedia/cyclopediainterfaces.h"
#include "gamewindow/cyclopediamapdialogcontroller.h"
#include "gamewindow/characterinfodialogcontroller.h"
#include "gamewindow/magicalarchivedialogcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "qmlcomponents/texthelper.h"

#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>

#include <QLocale>

#include <limits>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/tgamenotify.h>
#include <tibia/game/thingsession_forward.h>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;
using Tab = cip::tibia::gamewindow::TCyclopediaDialogController::DialogTab;

QString englishNumber(const qulonglong value) { return QLocale(QLocale::English).toString(value); }

/// The local player's balance of `type`, or nothing outside the game.
std::optional<qulonglong> balance(const tibia::client::ResourceTypes_t type)
{
    if (!tibia::game::hasLocalPlayer())
        return std::nullopt;
    return tibia::game::localPlayerResourceBalance(type);
}

QString balanceString(const tibia::client::ResourceTypes_t type, const char* fallback)
{
    const std::optional<qulonglong> value = balance(type);
    return value ? englishNumber(*value) : QString::fromLatin1(fallback);
}

}

TCyclopediaDialogController::TCyclopediaDialogController(QObject* parent)
    : TDialogController(QStringLiteral("CyclopediaDialog.qml"), parent)
    , m_bestiary(new cyclopedia::TBestiaryStorage(this))
    , m_itemInfo(new TItemInfoDialogController(this))
    , m_characterInfo(new TCharacterInfoDialogController(this))
    , m_monsters(new TMonsterDialogController(*m_bestiary, this))
    , m_bonusEffects(new TMonsterBonusEffectsDialogController(*m_bestiary, this))
    , m_bosstiaryStorage(new cyclopedia::TBosstiaryStorage(this))
    , m_bosstiary(new TBosstiaryDialogController(*m_bosstiaryStorage, this))
    , m_bossSlots(new TBossSlotsDialogController(*m_bosstiaryStorage, this))
    , m_houses(new THousesInfoDialogController(this))
    , m_map(new TCyclopediaMapDialogController(this))
    , m_magicalArchive(new TMagicalArchiveDialogController(this))
{
    m_itemInfoDialogController = m_itemInfo;
    m_monstersDialogController = m_monsters;
    m_characterInfoDialogController = m_characterInfo;
    m_bonusEffectsDialogController = m_bonusEffects;
    m_mapDialogController = m_map;
    m_housesDialogController = m_houses;
    m_bosstiaryDialogController = m_bosstiary;
    m_bossSlotsDialogController = m_bossSlots;
    m_magicalArchiveDialogController = m_magicalArchive;
    connect(m_itemInfo, &TItemInfoDialogController::publishGameAction, this, &TCyclopediaDialogController::publishGameAction);
    connect(m_characterInfo, &TCharacterInfoDialogController::publishGameAction, this, &TCyclopediaDialogController::publishGameAction);
    connect(m_monsters, &TMonsterDialogController::publishGameAction, this, &TCyclopediaDialogController::publishGameAction);
    connect(m_bonusEffects, &TMonsterBonusEffectsDialogController::publishGameAction, this, &TCyclopediaDialogController::publishGameAction);
    connect(m_houses, &THousesInfoDialogController::publishGameAction, this, &TCyclopediaDialogController::publishGameAction);
    connect(m_map, &TCyclopediaMapDialogController::publishGameAction, this, &TCyclopediaDialogController::publishGameAction);
    connect(m_magicalArchive, &TMagicalArchiveDialogController::publishGameAction, this, &TCyclopediaDialogController::publishGameAction);

    // Tip ICyclopediaDialogTab / IDialogTabPage twins (moc floor); live tabs
    // inherit cip::ICyclopediaDialogTab via their CIP T* bases.
    (void)tipICyclopediaDialogTab();
    (void)tipIDialogTabPage();

    hookActions();
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::resourcesBalanceChanged, this,
            [this](qint64, qint64, int type) {
        switch (type) {
            case tibia::client::RESOURCE_BANK_BALANCE:
            case tibia::client::RESOURCE_GOLD_EQUIPPED: Q_EMIT goldBalanceChanged(); break;
            case tibia::client::RESOURCE_CHARM: Q_EMIT availableCharmPointsChanged(); break;
            case tibia::client::RESOURCE_MAX_CHARM: Q_EMIT unlockedCharmPointsChanged(); break;
            case tibia::client::RESOURCE_MINOR_CHARM: Q_EMIT availableMinorCharmEchoesChanged(); break;
            case tibia::client::RESOURCE_MAX_MINOR_CHARM: Q_EMIT unlockedMinorCharmEchoesChanged(); break;
            default: break;
        }
    });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        clearBacklog();
        closeDialog();
    });
}

void TCyclopediaDialogController::setAmbientLightStorage(worldmap::TAmbientLightStorage* ambientLight)
{
    m_houses->setAmbientLightStorage(ambientLight);
    m_map->setAmbientLightStorage(ambientLight);
}

void TCyclopediaDialogController::setInstanceStorage(mods::InstanceBackend* instance) { m_map->setInstanceStorage(instance); }

void TCyclopediaDialogController::setControlScheme(const int controlScheme) { m_map->setControlScheme(controlScheme); }

TCyclopediaDialogController::~TCyclopediaDialogController()
{
}

QString TCyclopediaDialogController::bankGoldBalanceString() const
{
    const std::optional<qulonglong> value = balance(tibia::client::RESOURCE_BANK_BALANCE);
    return value ? englishNumber(*value) : qtTrId("dummy_unknown");
}

QString TCyclopediaDialogController::inventoryGoldBalanceString() const
{
    const std::optional<qulonglong> value = balance(tibia::client::RESOURCE_GOLD_EQUIPPED);
    return value ? englishNumber(*value) : qtTrId("dummy_unknown");
}

qulonglong TCyclopediaDialogController::totalGoldBalance() const
{
    return balance(tibia::client::RESOURCE_BANK_BALANCE).value_or(0) + balance(tibia::client::RESOURCE_GOLD_EQUIPPED).value_or(0);
}

QString TCyclopediaDialogController::totalGoldBalanceString() const
{
    if (!tibia::game::hasLocalPlayer())
        return qtTrId("dummy_unknown");
    qmlcomponents::TTextHelperQmlService textHelper;
    return textHelper.formatNumberWithThousandSeparatorsAndThousandShortcutsAsMultiString(static_cast<qlonglong>(totalGoldBalance()),
                                                                                         false);
}

QString TCyclopediaDialogController::availableCharmPointsString() const { return balanceString(tibia::client::RESOURCE_CHARM, "0"); }
QString TCyclopediaDialogController::unlockedCharmPointsString() const { return balanceString(tibia::client::RESOURCE_MAX_CHARM, "0"); }
QString TCyclopediaDialogController::availableMinorCharmEchoesString() const { return balanceString(tibia::client::RESOURCE_MINOR_CHARM, "0"); }
QString TCyclopediaDialogController::unlockedMinorCharmEchoesString() const { return balanceString(tibia::client::RESOURCE_MAX_MINOR_CHARM, "0"); }

void TCyclopediaDialogController::hookActions()
{
    m_gameActions.on(A::SHOW_BOSSTIARY_FREEPODIUM_DIALOG, [this](const input::IGameAction& action) {
        m_bosstiary->handleGameAction(action);
        return true;
    });
    m_gameActions.on(A::OPEN_CYCLOPEDIA, [this](const input::TGameActionOpenCyclopedia& action) {
        // The action's tab and the page it opens there.
        switch (action.tab()) {
            case 0: {
                captureNavigationBacklogContextIfNeedAndShowDialog(Tab::ItemInfo);
                if (const uint16_t typeId = action.typeId())
                    m_itemInfo->showType(typeId);
                break;
            }
            case 1:
                if (action.raceId() > 0)
                    m_monsters->setNextRace(action.raceId());
                captureNavigationBacklogContextIfNeedAndShowDialog(Tab::Monsters);
                break;
            case 2:
                if (action.category().value_or(0) > 0)
                    m_characterInfo->setNextDataType(*action.category());
                captureNavigationBacklogContextIfNeedAndShowDialog(Tab::CharacterInfo);
                break;
            case 3:
                // Charm 0 is a charm; no charm is 0xff.
                captureNavigationBacklogContextIfNeedAndShowDialog(Tab::BonusEffects);
                if (action.charmId() != 0xff)
                    m_bonusEffects->selectCharm(action.charmId());
                break;
            case 4: {
                captureNavigationBacklogContextIfNeedAndShowDialog(Tab::Map);
                const Position& coordinate = action.coordinate();
                if (coordinate.x != 0 || coordinate.y != 0 || coordinate.z != 0)
                    m_map->centerAt(coordinate, action.zoom());
                break;
            }
            case 5:
                // The houses page selects the action's house.
                captureNavigationBacklogContextIfNeedAndShowDialog(Tab::Houses);
                if (action.category().value_or(0) > 0)
                    m_houses->selectHouse(static_cast<uint>(*action.category()));
                break;
            case 6:
                captureNavigationBacklogContextIfNeedAndShowDialog(Tab::Bosstiary);
                if (action.raceId() > 0)
                    m_bosstiary->showRace(action.raceId());
                break;
            case 7: captureNavigationBacklogContextIfNeedAndShowDialog(Tab::BossSlots); break;
            case 8:
                // The magical archive selects the action's spell.
                captureNavigationBacklogContextIfNeedAndShowDialog(Tab::MagicalArchive);
                if (action.typeId() > 0)
                    m_magicalArchive->setSelectedSpellID(action.typeId());
                break;
            default: break;
        }
        return true;
    });
    m_gameActions.on(A::CLOSE_CYCLOPEDIA, [this](const input::IGameAction&) {
        clearBacklog();
        closeDialog();
        return true;
    });
}

void TCyclopediaDialogController::showDialog(const DialogTab& Tab)
{
    closeDialog();
    // CIP asks the server for the charm, bank and inventory balances.
    for (const int resource : { 30, 0, 1 }) {
        auto action = std::make_shared<input::TGameActionRequestResourceBalance>(resource);
        Q_EMIT publishGameAction(action);
    }
    if (!dialog()->isShown())
        dialog()->show();
    switchTab(Tab);
}

void TCyclopediaDialogController::captureNavigationBacklogContextIfNeedAndShowDialog(const DialogTab& NewTab)
{
    // Reopening the shown tab leaves no trace in the backlog.
    if (NewTab != m_dialogTab)
        pushBacklog();
    showDialog(NewTab);
}

void TCyclopediaDialogController::closeDialog()
{
    cip::tibia::gamewindow::ICyclopediaDialogTab* previous = page(m_dialogTab);
    m_dialogTab = NoTabInitialState;
    Q_EMIT dialogTabChanged();
    if (previous)
        previous->hideDialogTab();
    dialog()->close();
}

void TCyclopediaDialogController::requestClose()
{
    clearBacklog();
    closeDialog();
}

void TCyclopediaDialogController::requestTabSwitch(const DialogTab& NewTab)
{
    if (NewTab == m_dialogTab)
        return;
    pushBacklog();
    switchTab(NewTab);
}

void TCyclopediaDialogController::requestOpenManageLootContainerDialog()
{
    clearBacklog();
    closeDialog();
    Q_EMIT publishGameAction(input::makeGameAction(A::OPEN_MANAGE_LOOT_CONTAINER));
}

void TCyclopediaDialogController::requestBack()
{
    if (m_backlog.empty())
        return;
    const Context context = m_backlog.back();
    m_backlog.pop_back();
    // The page gets its context back. Pages that rebuild on showDialogTab
    // take it as pending before the switch; the item page takes it after.
    if (context.tab == CharacterInfo)
        m_characterInfo->setNextDataType(context.characterInfo.dataType);
    if (context.tab == Monsters)
        m_monsters->restoreContext(context.monsters);
    if (context.tab == BonusEffects)
        m_bonusEffects->restoreContext(context.bonusEffects);
    if (context.tab == Bosstiary)
        m_bosstiary->restoreContext(context.bosstiary);
    if (context.tab == Houses)
        m_houses->restoreContext(context.houses);
    if (context.tab == MagicalArchive)
        m_magicalArchive->restoreContext(context.magicalArchiveSpellId);
    switchTab(context.tab);
    if (context.tab == ItemInfo)
        m_itemInfo->restoreContext(context.itemInfo);
    if (context.tab == CharacterInfo)
        m_characterInfo->restoreContext(context.characterInfo);
    Q_EMIT navigationBacklogChanged();
}

void TCyclopediaDialogController::switchTab(const DialogTab tab)
{
    // The new page shows before the tab changes; the old one hides last. A
    // tab without a page closes to the initial state.
    cip::tibia::gamewindow::ICyclopediaDialogTab* previous = page(m_dialogTab);
    cip::tibia::gamewindow::ICyclopediaDialogTab* next = page(tab);
    if (next)
        next->showDialogTab();
    m_dialogTab = next ? tab : NoTabInitialState;
    Q_EMIT dialogTabChanged();
    if (previous)
        previous->hideDialogTab();
}

cip::tibia::gamewindow::ICyclopediaDialogTab* TCyclopediaDialogController::page(const DialogTab tab) const
{
    // Page controllers inherit cip::ICyclopediaDialogTab (via CIP T* bases).
    using cip::tibia::gamewindow::ICyclopediaDialogTab;
    switch (tab) {
        case ItemInfo: return qobject_cast<ICyclopediaDialogTab*>(m_itemInfo);
        case Monsters: return qobject_cast<ICyclopediaDialogTab*>(m_monsters);
        case BonusEffects: return qobject_cast<ICyclopediaDialogTab*>(m_bonusEffects);
        case Map: return qobject_cast<ICyclopediaDialogTab*>(m_map);
        case CharacterInfo: return qobject_cast<ICyclopediaDialogTab*>(m_characterInfo);
        case Houses: return qobject_cast<ICyclopediaDialogTab*>(m_houses);
        case Bosstiary: return qobject_cast<ICyclopediaDialogTab*>(m_bosstiary);
        case BossSlots: return qobject_cast<ICyclopediaDialogTab*>(m_bossSlots);
        case MagicalArchive: return qobject_cast<ICyclopediaDialogTab*>(m_magicalArchive);
        default: return nullptr;
    }
}

TCyclopediaDialogController::Context TCyclopediaDialogController::captureContext() const
{
    Context context;
    context.tab = m_dialogTab;
    if (m_dialogTab == ItemInfo)
        context.itemInfo = m_itemInfo->captureContext();
    if (m_dialogTab == CharacterInfo)
        context.characterInfo = m_characterInfo->captureContext();
    if (m_dialogTab == Monsters)
        context.monsters = m_monsters->captureContext();
    if (m_dialogTab == BonusEffects)
        context.bonusEffects = m_bonusEffects->captureContext();
    if (m_dialogTab == Bosstiary)
        context.bosstiary = m_bosstiary->captureContext();
    if (m_dialogTab == Houses)
        context.houses = m_houses->captureContext();
    if (m_dialogTab == MagicalArchive)
        context.magicalArchiveSpellId = m_magicalArchive->captureContext();
    return context;
}

void TCyclopediaDialogController::pushBacklog()
{
    if (!page(m_dialogTab))
        return;
    const Context context = captureContext();
    if (!m_backlog.empty() && m_backlog.back() == context)
        return;
    m_backlog.push_back(context);
    // At most 100 steps back; the oldest falls out.
    if (m_backlog.size() > MaxBacklog)
        m_backlog.pop_front();
    Q_EMIT navigationBacklogChanged();
}

void TCyclopediaDialogController::clearBacklog()
{
    m_backlog.clear();
    Q_EMIT navigationBacklogChanged();
}

TCyclopediaButtonController::TCyclopediaButtonController(QObject* parent)
    : cip::tibia::gamewindow::TCyclopediaButtonController(parent)
{
    // A single-shot ten-minute highlight; its end forgets what a click opens.
    m_highlightTimer.setSingleShot(true);
    m_highlightTimer.setInterval(600000);
    connect(&m_highlightTimer, &QTimer::timeout, this, &TCyclopediaButtonController::changed);
    connect(&m_highlightTimer, &QTimer::timeout, this, &TCyclopediaButtonController::onHighlightCyclopediaButtonEnds);
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onBestiaryEntryChanged", [this](GameObject*, const std::vector<std::any>& args) {
        onMonsterCyclopediaNewDetailsMessage(static_cast<uint16_t>(client::gamecore::argInt(args, 0)));
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onCyclopediaMapRaid", [this](GameObject*, const std::vector<std::any>& args) {
        using client::gamecore::argInt;
        onCyclopediaMapActiveRaidAdded(worldmap::TWorldMapCoordinate(static_cast<int>(argInt(args, 0)), static_cast<int>(argInt(args, 1)),
                                                                     static_cast<int>(argInt(args, 2))));
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onCyclopediaMapImminentRaid", [this](GameObject*, const std::vector<std::any>& args) {
        using client::gamecore::argInt;
        const worldmap::TWorldMapCoordinate coordinate(static_cast<int>(argInt(args, 0)), static_cast<int>(argInt(args, 1)),
                                                       static_cast<int>(argInt(args, 2)));
        const auto* scale = args.size() > 3 ? std::any_cast<double>(&args[3]) : nullptr;
        onCyclopediaMapImminentRaidsChanged(coordinate, scale ? static_cast<float>(*scale) : 0.0f);
        return std::any{};
    });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameStarted, this,
            &TCyclopediaButtonController::connectToGameSession);
}

void TCyclopediaButtonController::onCyclopediaButtonClicked()
{
    std::shared_ptr<input::TGameActionOpenCyclopedia> action;
    if (const auto* raceId = std::get_if<uint16_t>(&m_highlightReason)) {
        action = std::make_shared<input::TGameActionOpenCyclopedia>(1);
        action->setRaceId(*raceId);
    } else if (const auto* raid = std::get_if<worldmap::TWorldMapCoordinate>(&m_highlightReason)) {
        action = std::make_shared<input::TGameActionOpenCyclopedia>(4);
        action->setCoordinate(raid->toPosition());
    } else if (const auto* imminent = std::get_if<TImminentRaid>(&m_highlightReason)) {
        action = std::make_shared<input::TGameActionOpenCyclopedia>(4);
        action->setCoordinate(imminent->coordinate.toPosition());
    } else {
        action = std::make_shared<input::TGameActionOpenCyclopedia>(0);
    }
    Q_EMIT publishGameAction(action);
    stopHighlight();
}

void TCyclopediaButtonController::onHighlightCyclopediaButtonEnds() { stopHighlight(); }

void TCyclopediaButtonController::connectToGameSession() { stopHighlight(); }

void TCyclopediaButtonController::onMonsterCyclopediaNewDetailsMessage(const uint16_t NewDetailsMonsterRaceID)
{
    m_highlightReason = NewDetailsMonsterRaceID;
    startHighlight();
}

void TCyclopediaButtonController::onCyclopediaMapActiveRaidAdded(const worldmap::TWorldMapCoordinate& WorldMapCoordinate)
{
    m_highlightReason = WorldMapCoordinate;
    startHighlight();
}

void TCyclopediaButtonController::onCyclopediaMapImminentRaidsChanged(const worldmap::TWorldMapCoordinate& WorldMapCoordinate,
                                                                      const float MapScaleFactor)
{
    m_highlightReason = TImminentRaid{ WorldMapCoordinate, MapScaleFactor };
    startHighlight();
}

void TCyclopediaButtonController::startHighlight()
{
    m_highlightTimer.start();
    Q_EMIT changed();
}

void TCyclopediaButtonController::stopHighlight()
{
    m_highlightTimer.stop();
    Q_EMIT changed();
    m_highlightReason = std::monostate{};
}

TBosstiaryButtonController::TBosstiaryButtonController(QObject* parent)
    : cip::tibia::gamewindow::TBosstiaryButtonController(parent)
{
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameStarted, this,
            &TBosstiaryButtonController::connectToGameSession);
    connectToGameSession();
}

void TBosstiaryButtonController::onOpenBosstiaryButtonClicked()
{
    auto action = std::make_shared<input::TGameActionOpenCyclopedia>(6);
    action->setRaceId(static_cast<uint16_t>(m_highlightedBossRaceId));
    Q_EMIT publishGameAction(action);
    if (m_highlightedBossRaceId != 0) {
        m_highlightedBossRaceId = 0;
        Q_EMIT changed();
    }
}

void TBosstiaryButtonController::connectToGameSession()
{
    if (m_highlightedBossRaceId != 0) {
        m_highlightedBossRaceId = 0;
        Q_EMIT changed();
    }
    m_hooks.clear();
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onBosstiaryEntryChanged", [this](GameObject*, const std::vector<std::any>& args) {
        onHighlightRequested(static_cast<uint16_t>(client::gamecore::argInt(args, 0)));
        return std::any{};
    });
}

void TBosstiaryButtonController::onHighlightRequested(const uint16_t BossRaceID)
{
    if (m_highlightedBossRaceId == BossRaceID)
        return;
    m_highlightedBossRaceId = BossRaceID;
    Q_EMIT changed();
}


void TCyclopediaDialogController::handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
{
    m_gameActions.handle(pGameAction);
    if (m_map)
        m_map->handleGameAction(pGameAction);
}

}
