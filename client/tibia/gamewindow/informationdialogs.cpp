#include "informationdialogs.h"

#include "config/clientoptions.h"
#include "gamewindow/configurecreaturepodiumdialogcontroller.h"
#include "gamewindow/dialogmanager.h"
#include "gamewindow/cyclopediadialogcontroller.h"
#include "gamewindow/cyclopediamapdialogcontroller.h"
#include "cyclopedia/cyclopediamapdata.h"
#include "gamewindow/edittextcontroller.h"
#include "gamewindow/highscoresdialogcontroller.h"
#include "gamewindow/infobannercontroller.h"
#include "gamewindow/linkcopywarningdialogcontroller.h"
#include "gamewindow/outfitdialogcontroller.h"
#include "gamewindow/preydialogcontroller.h"
#include "prey/preyciptypes.h"
#include "gamewindow/questlogcontroller.h"
#include "questlog/questlogciptypes.h"
#include "gamewindow/remainingdialogs.h"
#include "gamewindow/servermodaldialogcontroller.h"
#include "input/gameactionhandlers.h"
#include "qmlcomponents/minimaprendererquickitem.h"

#include <tibia/game/gamesession.h>
#include <map>
#include <optional>
#include <client/outfit.h>
#include <client/position.h>
#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>
#include <tibia/game/gamenotify.h>
#include <tibia/game/tgamenotify.h>
#include <tibia/game/thingsession_forward.h>

namespace tibia::gamewindow {

namespace {

using Args = std::vector<std::any>;
using A = input::EGameAction;
using Entry = TCustomiseCharacterModel::Entry;

template<class T>
const T* arg(const Args& args, const size_t index)
{
    return index < args.size() ? std::any_cast<T>(&args[index]) : nullptr;
}

QString qstr(const std::string& text)
{
    return QString::fromStdString(text);
}

/// The engine and the dialog parent item the dialogs are made with.
QQmlEngine* dialogEngine()
{
    TDialogStackController* const stack = TDialogStackController::instance();
    return stack ? stack->engine() : nullptr;
}

QQuickItem* dialogParentItem()
{
    TDialogStackController* const stack = TDialogStackController::instance();
    return stack ? stack->dialogPlaceholder() : nullptr;
}

}

TInformationDialogs::TInformationDialogs(QObject* parent)
    : QObject(parent)
    , m_outfitDialog(new TSelectOutfitDialogController(this))
    , m_podiumDialog(new TConfigureCreaturePodiumDialogController(this))
    , m_modalDialog(new TServerModalDialogController(dialogEngine(), dialogParentItem(), this))
    , m_editText(new TEditTextController(nullptr, nullptr, this))
    , m_questLog(new TQuestLogController(dialogEngine(), dialogParentItem(), this))
    , m_highscores(new THighscoresDialogController(this))
    , m_prey(new TPreyDialogController(config::TClientOptions::instance(), dialogEngine(), dialogParentItem(), this))
    , m_cyclopedia(new TCyclopediaDialogController(this))
    , m_cyclopediaButton(new TCyclopediaButtonController(this))
    , m_bosstiaryButton(new TBosstiaryButtonController(this))
{
    // The dialogs' publishGameAction reach the game window's router through this hub.
    const auto forward = [this](auto* dialog) {
        connect(dialog, &std::remove_pointer_t<decltype(dialog)>::publishGameAction, this, &TInformationDialogs::publishGameAction);
    };
    forward(m_outfitDialog);
    forward(m_podiumDialog);
    forward(m_editText);
    forward(m_questLog);
    forward(m_prey);
    forward(m_cyclopedia);
    forward(m_cyclopediaButton);
    forward(m_bosstiaryButton);
    // CIP misc dialogs / overlays kept alive on the information hub (ledger wires).
    new TEmergencyDialogController(this);
    new TCinematicSessiondumpPlayerController(this);
    new TGenericConfirmWithDontShowAgainDialogController(this);
    auto* linkCopyWarning = new TLinkCopyWarningDialogController(this);
    forward(linkCopyWarning);
    m_gameActions.on(A::REQUEST_CONFORMATION_FOR_COPYING_TEXT_TO_CLIPBOARD,
                     [linkCopyWarning](const input::IGameAction& action) { linkCopyWarning->handleGameAction(action); });
    new TLoginConfirmationCodeEnterTextDialogController(this);
    new TTwoFactorCodeEnterTextDialogController(this);
    new TReturnerRewardDialogController(this);
    forward(new TSurveyInvitationDialogController(this));
    new TInfoBannerController(nullptr, this);

    hookOutfitDialog();
    hookCreaturePodium();
    hookTextDialogs();
    hookQuestLog();
    hookHighscores();
    hookPrey();
}

TInformationDialogs::~TInformationDialogs()
{
}

void TInformationDialogs::setAmbientLightStorage(worldmap::TAmbientLightStorage* ambientLight)
{
    m_cyclopedia->setAmbientLightStorage(ambientLight);
}

void TInformationDialogs::setMinimapTileStorage(minimap::TMinimapTileStorage* tiles) { qmlcomponents::TMinimapRendererQuickItem::setMinimapTileStorage(tiles); }

void TInformationDialogs::setInstanceStorage(mods::InstanceBackend* instance) { m_cyclopedia->setInstanceStorage(instance); }

void TInformationDialogs::setControlScheme(const int controlScheme) { m_cyclopedia->setControlScheme(controlScheme); }

void TInformationDialogs::hookOutfitDialog()
{
    // CIP: REQUEST_OUTFITS_FROM_SERVER is the session's TPlayerGameActionHandler's; the
    // outfit dialog follows the session (TSelectOutfitDialogController::connectToGameSession).
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() { m_questLog->clear(); });
}

void TInformationDialogs::hookCreaturePodium()
{
    using RaceRows = std::vector<TConfigureCreaturePodiumDialogController::RaceRow>;
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onConfigureCreaturePodium", [this](GameObject*, const Args& args) {
        static const RaceRows emptyRaces;
        const Outfit selected = arg<Outfit>(args, 0) ? *arg<Outfit>(args, 0) : Outfit{};
        const bool bossPodium = client::gamecore::argInt(args, 1) != 0;
        const auto* races = arg<RaceRows>(args, 2);
        const Position pos = arg<Position>(args, 3) ? *arg<Position>(args, 3) : Position{};
        m_podiumDialog->open(selected, static_cast<uint16_t>(client::gamecore::argInt(args, 9)), bossPodium, races ? *races : emptyRaces, pos,
                             static_cast<uint16_t>(client::gamecore::argInt(args, 4)),
                             static_cast<uint8_t>(client::gamecore::argInt(args, 5)),
                             client::gamecore::argInt(args, 6) != 0, client::gamecore::argInt(args, 7) != 0,
                             static_cast<uint8_t>(client::gamecore::argInt(args, 8)));
        return std::any{};
    });
}

void TInformationDialogs::hookTextDialogs()
{
    using client::gamecore::argInt;
    using client::gamecore::argString;

    m_hooks.emplace_back(tibia::game::kNotifyScope, "onModalDialog", [this](GameObject*, const Args& args) {
        using Options = TServerModalDialogController::Options;
        static const Options none;
        const auto* buttons = arg<Options>(args, 3);
        const auto* choices = arg<Options>(args, 6);
        m_modalDialog->open(argInt(args, 0), argString(args, 1), argString(args, 2), buttons ? *buttons : none, argInt(args, 4),
                            argInt(args, 5), choices ? *choices : none);
        return std::any{};
    });
}

void TInformationDialogs::hookQuestLog()
{
    m_gameActions.on(A::OPEN_QUESTLOG_DIALOG, [this](const input::TGameActionOpenQuestLog& action) {
        m_questLog->showDialog(action.questId());
        return true;
    });

}

void TInformationDialogs::hookHighscores()
{
    m_gameActions.on(A::OPEN_HIGHSCORES_DIALOG, [this](const input::IGameAction&) {
        m_highscores->openDialog();
        return true;
    });
}

namespace {

prey::TPreyMonster preyMonster(const std::string& name, const Outfit& outfit)
{
    return { .name = QString::fromStdString(name),
             .outfitId = outfit.getId(),
             .head = outfit.getHead(),
             .torso = outfit.getBody(),
             .legs = outfit.getLegs(),
             .detail = outfit.getFeet(),
             .firstAddOn = (outfit.getAddons() & 1) != 0,
             .secondAddOn = (outfit.getAddons() & 2) != 0 };
}

QList<prey::TPreyMonster> preyMonsters(const Args& args, const size_t namesIndex, const size_t outfitsIndex)
{
    QList<prey::TPreyMonster> monsters;
    const auto* names = arg<std::vector<std::string>>(args, namesIndex);
    const auto* outfits = arg<std::vector<Outfit>>(args, outfitsIndex);
    if (!names || !outfits)
        return monsters;
    for (size_t i = 0; i < std::min(names->size(), outfits->size()); ++i)
        monsters.append(preyMonster((*names)[i], (*outfits)[i]));
    return monsters;
}

QList<uint16_t> preyRaces(const Args& args, const size_t index)
{
    const auto* races = arg<std::vector<uint16_t>>(args, index);
    return races ? QList<uint16_t>(races->begin(), races->end()) : QList<uint16_t>{};
}

prey::TPreyBonus preyBonus(const Args& args, const size_t index)
{
    using client::gamecore::argInt;
    return { .type = static_cast<int>(argInt(args, index)), .value = static_cast<int>(argInt(args, index + 1)),
             .grade = static_cast<int>(argInt(args, index + 2)) };
}

}

void TInformationDialogs::hookPrey()
{
    using client::gamecore::argInt;
    using client::gamecore::argString;

    m_gameActions.on(A::OPEN_PREY_DIALOG, [this](const input::IGameAction&) {
        m_prey->showDialog();
        return true;
    });
    // Every slot state ends with the free list reroll (seconds from now) and
    // the automatic extension option (CIP's "wildcards" outside the
    // active state).
    const auto slotData = [this](const Args& args, const size_t rerollIndex, const size_t optionIndex) -> int {
        const int preyId = static_cast<int>(argInt(args, 0));
        m_prey->storage()->setSlotNextFreeListReroll(preyId, argInt(args, rerollIndex));
        m_prey->storage()->setSlotAutomaticExtendType(preyId, static_cast<int>(argInt(args, optionIndex)));
        return preyId;
    };

    m_hooks.emplace_back(tibia::game::kNotifyScope, "onPreyLocked", [this, slotData](GameObject*, const Args& args) {
        const int preyId = slotData(args, 2, 3);
        const auto* unlock = arg<tibia::client::PreyUnlockState_t>(args, 1);
        m_prey->storage()->setSlotLocked(preyId, unlock ? *unlock : tibia::client::PREY_UNLOCK_NONE);
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onPreyInactive", [this, slotData](GameObject*, const Args& args) {
        m_prey->storage()->setSlotInactive(slotData(args, 1, 2));
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onPreyActive", [this, slotData](GameObject*, const Args& args) {
        const int preyId = slotData(args, 7, 9);
        const auto* outfit = arg<Outfit>(args, 2);
        m_prey->storage()->setSlotActive(preyId, preyBonus(args, 3),
                                         preyMonster(argString(args, 1).toStdString(), outfit ? *outfit : Outfit{}),
                                         static_cast<int>(argInt(args, 6)));
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onPreySelection", [this, slotData](GameObject*, const Args& args) {
        m_prey->storage()->setSlotSelection(slotData(args, 3, 4), preyMonsters(args, 1, 2));
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onPreySelectionChangeMonster", [this, slotData](GameObject*, const Args& args) {
        m_prey->storage()->setSlotSelectionChangeMonster(slotData(args, 6, 7), preyBonus(args, 3), preyMonsters(args, 1, 2));
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onPreyListSelection", [this, slotData](GameObject*, const Args& args) {
        m_prey->storage()->setSlotListSelection(slotData(args, 2, 3), preyRaces(args, 1));
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onPreyWildcardSelection", [this, slotData](GameObject*, const Args& args) {
        m_prey->storage()->setSlotWildcardSelection(slotData(args, 2, 3), preyBonus(args, 4), preyRaces(args, 1));
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onPreyTimeLeft", [this](GameObject*, const Args& args) {
        m_prey->storage()->setSlotTimeLeft(static_cast<int>(argInt(args, 0)), static_cast<int>(argInt(args, 1)));
        return std::any{};
    });
    // CIP PreyFreeRerolls (S 0xEE): next free list reroll countdown for one slot.
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onPreyFreeRerolls", [this](GameObject*, const Args& args) {
        m_prey->storage()->setSlotNextFreeListReroll(static_cast<int>(argInt(args, 0)), argInt(args, 1));
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onPreyRerollPrice", [this](GameObject*, const Args& args) {
        m_prey->storage()->setPrices(static_cast<int>(argInt(args, 0)), static_cast<int>(argInt(args, 1)), static_cast<int>(argInt(args, 2)));
        return std::any{};
    });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::resourcesBalanceChanged, this,
            [this]() {
        m_prey->onBalanceChanged();
    });
}


void TInformationDialogs::handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
{
    m_gameActions.handle(pGameAction);
    if (m_cyclopedia)
        m_cyclopedia->handleGameAction(pGameAction);
}

}
