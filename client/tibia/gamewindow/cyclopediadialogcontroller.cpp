#include "cyclopediadialogcontroller.h"

#include "cyclopedia/bestiarystorage.h"
#include "gamewindow/cyclopediamapdialogcontroller.h"
#include "gamewindow/characterinfodialogcontroller.h"
#include "gamewindow/magicalarchivedialogcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "qmlcomponents/texthelper.h"

#include <tibia/world/game.h>
#include <tibia/world/localplayer.h>
#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>

#include <QLocale>

#include <limits>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;
using Tab = cip::tibia::gamewindow::TCyclopediaDialogController::DialogTab;

QString englishNumber(const qulonglong value) { return QLocale(QLocale::English).toString(value); }

/// The local player's balance of `type`, or nothing outside the game.
std::optional<qulonglong> balance(const Otc::ResourceTypes_t type)
{
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    if (!player)
        return std::nullopt;
    return player->getResourceBalance(type);
}

/// The item type an OPEN_CYCLOPEDIA action names: its `instance`, or the
/// Cyclopedia type of its object (context menus).
uint16_t cyclopediaTypeOf(const input::TGameAction& action)
{
    if (action.instance > 0)
        return static_cast<uint16_t>(action.instance);
    if (!action.object || !action.object->isItem())
        return 0;
    const ThingTypePtr& type = g_things.getThingType(action.object->getId(), ThingCategoryItem);
    if (!type || !type->hasCyclopediaItem())
        return 0;
    return static_cast<uint16_t>(type->getCyclopediaType() ? type->getCyclopediaType() : type->getId());
}

QString balanceString(const Otc::ResourceTypes_t type, const char* fallback)
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

    hookActions();
    m_hooks.emplace_back("g_game", "onResourcesBalanceChange", [this](LuaObject*, const std::vector<std::any>& args) {
        const auto* type = args.size() > 2 ? std::any_cast<Otc::ResourceTypes_t>(&args[2]) : nullptr;
        switch (type ? *type : -1) {
            case Otc::RESOURCE_BANK_BALANCE:
            case Otc::RESOURCE_GOLD_EQUIPPED: Q_EMIT goldBalanceChanged(); break;
            case Otc::RESOURCE_CHARM: Q_EMIT availableCharmPointsChanged(); break;
            case Otc::RESOURCE_MAX_CHARM: Q_EMIT unlockedCharmPointsChanged(); break;
            case Otc::RESOURCE_MINOR_CHARM: Q_EMIT availableMinorCharmEchoesChanged(); break;
            case Otc::RESOURCE_MAX_MINOR_CHARM: Q_EMIT unlockedMinorCharmEchoesChanged(); break;
            default: break;
        }
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onGameEnd", [this](LuaObject*, const std::vector<std::any>&) {
        clearBacklog();
        closeDialog();
        return std::any{};
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
    input::setActionHandler(A::OPEN_CYCLOPEDIA, nullptr);
    input::setActionHandler(A::CLOSE_CYCLOPEDIA, nullptr);
}

QString TCyclopediaDialogController::bankGoldBalanceString() const
{
    const std::optional<qulonglong> value = balance(Otc::RESOURCE_BANK_BALANCE);
    return value ? englishNumber(*value) : qtTrId("dummy_unknown");
}

QString TCyclopediaDialogController::inventoryGoldBalanceString() const
{
    const std::optional<qulonglong> value = balance(Otc::RESOURCE_GOLD_EQUIPPED);
    return value ? englishNumber(*value) : qtTrId("dummy_unknown");
}

qulonglong TCyclopediaDialogController::totalGoldBalance() const
{
    return balance(Otc::RESOURCE_BANK_BALANCE).value_or(0) + balance(Otc::RESOURCE_GOLD_EQUIPPED).value_or(0);
}

QString TCyclopediaDialogController::totalGoldBalanceString() const
{
    qmlcomponents::TTextHelperQmlService textHelper;
    return textHelper.formatNumberWithThousandSeparatorsAndThousandShortcutsAsMultiString(static_cast<qlonglong>(totalGoldBalance()));
}

QString TCyclopediaDialogController::availableCharmPointsString() const { return balanceString(Otc::RESOURCE_CHARM, "0"); }
QString TCyclopediaDialogController::unlockedCharmPointsString() const { return balanceString(Otc::RESOURCE_MAX_CHARM, "0"); }
QString TCyclopediaDialogController::availableMinorCharmEchoesString() const { return balanceString(Otc::RESOURCE_MINOR_CHARM, "0"); }
QString TCyclopediaDialogController::unlockedMinorCharmEchoesString() const { return balanceString(Otc::RESOURCE_MAX_MINOR_CHARM, "0"); }

void TCyclopediaDialogController::hookActions()
{
    input::setActionHandler(A::OPEN_CYCLOPEDIA, [this](const input::TGameAction& action) {
        // CIP 0x100cf1260: the action's tab type and its sub-page.
        switch (action.parameter) {
            case 0: {
                captureNavigationBacklogContextIfNeedAndShowDialog(Tab::ItemInfo);
                if (const uint16_t typeId = cyclopediaTypeOf(action))
                    m_itemInfo->showType(typeId);
                break;
            }
            case 1:
                if (action.category > 0)
                    m_monsters->setNextRace(action.category);
                captureNavigationBacklogContextIfNeedAndShowDialog(Tab::Monsters);
                break;
            case 2:
                if (action.category > 0)
                    m_characterInfo->setNextDataType(action.category);
                captureNavigationBacklogContextIfNeedAndShowDialog(Tab::CharacterInfo);
                break;
            case 3:
                captureNavigationBacklogContextIfNeedAndShowDialog(Tab::BonusEffects);
                if (action.category > 0)
                    m_bonusEffects->selectCharm(action.category);
                break;
            case 4:
                captureNavigationBacklogContextIfNeedAndShowDialog(Tab::Map);
                if (action.position.x != 0 || action.position.y != 0)
                    m_map->centerAt(action.position, std::numeric_limits<float>::quiet_NaN());
                break;
            case 5: captureNavigationBacklogContextIfNeedAndShowDialog(Tab::Houses); break;
            case 6:
                captureNavigationBacklogContextIfNeedAndShowDialog(Tab::Bosstiary);
                if (action.category > 0)
                    m_bosstiary->showRace(action.category);
                break;
            case 7: captureNavigationBacklogContextIfNeedAndShowDialog(Tab::BossSlots); break;
            case 8: captureNavigationBacklogContextIfNeedAndShowDialog(Tab::MagicalArchive); break;
            default: break;
        }
        return true;
    });
    input::setActionHandler(A::CLOSE_CYCLOPEDIA, [this](const input::TGameAction&) {
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
        auto action = input::mutableClone(*input::makeAction(A::REQUEST_RESOURCE_BALANCE));
        action->parameter = resource;
        input::publish(action);
    }
    if (!dialog()->isShown())
        dialog()->show();
    switchTab(Tab);
    Q_EMIT goldBalanceChanged();
    Q_EMIT availableCharmPointsChanged();
    Q_EMIT unlockedCharmPointsChanged();
    Q_EMIT availableMinorCharmEchoesChanged();
    Q_EMIT unlockedMinorCharmEchoesChanged();
}

void TCyclopediaDialogController::captureNavigationBacklogContextIfNeedAndShowDialog(const DialogTab& NewTab)
{
    pushBacklog();
    showDialog(NewTab);
}

void TCyclopediaDialogController::closeDialog()
{
    hideCurrentPage();
    m_dialogTab = NoTabInitialState;
    Q_EMIT dialogTabChanged();
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

void TCyclopediaDialogController::requestOpenManageQuickLootDialog()
{
    clearBacklog();
    closeDialog();
    input::publish(input::makeAction(A::OPEN_MANGE_LOOT_CONTAINER));
}

void TCyclopediaDialogController::requestBack()
{
    if (m_backlog.empty())
        return;
    const Context context = m_backlog.back();
    m_backlog.pop_back();
    Q_EMIT navigationBacklogChanged();
    if (context.tab == CharacterInfo && context.characterInfoDataType > 0)
        m_characterInfo->setNextDataType(context.characterInfoDataType);
    if (context.tab == Monsters)
        m_monsters->restoreContext(context.monsters);
    if (context.tab == BonusEffects)
        m_bonusEffects->restoreContext(context.bonusEffects);
    if (context.tab == Bosstiary)
        m_bosstiary->restoreContext(context.bosstiary);
    if (context.tab == Houses)
        m_houses->restoreContext(context.houses);
    if (!dialog()->isShown())
        showDialog(context.tab);
    else
        switchTab(context.tab);
    if (context.tab == ItemInfo)
        m_itemInfo->restoreContext(context.itemInfo);
}

void TCyclopediaDialogController::switchTab(const DialogTab tab)
{
    hideCurrentPage();
    m_dialogTab = tab;
    Q_EMIT dialogTabChanged();
    if (auto* current = page(tab))
        current->showDialogTab();
}

cip::tibia::gamewindow::ICyclopediaDialogTab* TCyclopediaDialogController::page(const DialogTab tab) const
{
    using cip::tibia::gamewindow::ICyclopediaDialogTab;
    switch (tab) {
        case ItemInfo: return qobject_cast<ICyclopediaDialogTab*>(m_itemInfoDialogController);
        case Monsters: return qobject_cast<ICyclopediaDialogTab*>(m_monstersDialogController);
        case BonusEffects: return qobject_cast<ICyclopediaDialogTab*>(m_bonusEffectsDialogController);
        case Map: return qobject_cast<ICyclopediaDialogTab*>(m_mapDialogController);
        case CharacterInfo: return m_characterInfo;
        case Houses: return qobject_cast<ICyclopediaDialogTab*>(m_housesDialogController);
        case Bosstiary: return qobject_cast<ICyclopediaDialogTab*>(m_bosstiaryDialogController);
        case BossSlots: return qobject_cast<ICyclopediaDialogTab*>(m_bossSlotsDialogController);
        case MagicalArchive: return qobject_cast<ICyclopediaDialogTab*>(m_magicalArchiveDialogController);
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
        context.characterInfoDataType = m_characterInfo->dataType();
    if (m_dialogTab == Monsters)
        context.monsters = m_monsters->captureContext();
    if (m_dialogTab == BonusEffects)
        context.bonusEffects = m_bonusEffects->captureContext();
    if (m_dialogTab == Bosstiary)
        context.bosstiary = m_bosstiary->captureContext();
    if (m_dialogTab == Houses)
        context.houses = m_houses->captureContext();
    return context;
}

void TCyclopediaDialogController::pushBacklog()
{
    if (m_dialogTab == NoTabInitialState || !dialog()->isShown())
        return;
    const Context context = captureContext();
    if (!m_backlog.empty() && m_backlog.back() == context)
        return;
    m_backlog.push_back(context);
    Q_EMIT navigationBacklogChanged();
}

void TCyclopediaDialogController::clearBacklog()
{
    if (m_backlog.empty())
        return;
    m_backlog.clear();
    Q_EMIT navigationBacklogChanged();
}

void TCyclopediaDialogController::hideCurrentPage()
{
    if (auto* current = page(m_dialogTab))
        current->hideDialogTab();
}

TCyclopediaButtonController::TCyclopediaButtonController(QObject* parent)
    : cip::tibia::gamewindow::TCyclopediaButtonController(parent)
{
    // CIP 0x1007d9cf0 with 0x927c0: a single-shot ten-minute highlight.
    m_highlightTimer.setSingleShot(true);
    m_highlightTimer.setInterval(600000);
    connect(&m_highlightTimer, &QTimer::timeout, this, &TCyclopediaButtonController::changed);
    m_hooks.emplace_back("g_game", "onBestiaryEntryChanged", [this](LuaObject*, const std::vector<std::any>& args) {
        m_newDetailsRaceId = client::gamecore::argInt(args, 0);
        m_highlightTimer.start();
        Q_EMIT changed();
        return std::any{};
    });
    // CIP 0x100cf3db0: a new session clears the highlight.
    m_hooks.emplace_back("g_game", "onGameStart", [this](LuaObject*, const std::vector<std::any>&) {
        stopHighlight();
        return std::any{};
    });
}

// CIP 0x100cf4420: the page of the highlight, else the item page.
void TCyclopediaButtonController::onCyclopediaButtonClicked()
{
    auto action = input::mutableClone(*input::makeAction(A::OPEN_CYCLOPEDIA));
    if (m_newDetailsRaceId > 0) {
        action->parameter = 1;
        action->category = m_newDetailsRaceId;
    } else {
        action->parameter = 0;
    }
    input::publish(action);
    stopHighlight();
}

void TCyclopediaButtonController::onHighlightCyclopediaButtonEnds() { stopHighlight(); }

void TCyclopediaButtonController::stopHighlight()
{
    m_highlightTimer.stop();
    m_newDetailsRaceId = 0;
    Q_EMIT changed();
}

void TBosstiaryButtonController::onOpenBosstiaryButtonClicked()
{
    auto action = input::mutableClone(*input::makeAction(A::OPEN_CYCLOPEDIA));
    action->parameter = 6;
    input::publish(action);
    if (m_highlighted) {
        m_highlighted = false;
        Q_EMIT changed();
    }
}

}
