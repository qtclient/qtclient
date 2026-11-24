#include "actionbarmanager.h"
#include "game/currentgamesession.h"
#include "game/gamesessionbase.h"
#include "input/keyboardshortcuthandler.h"

#include "config/clientoptions.h"
#include "game/gameactionrouter.h"
#include "game/gameciptypes.h"
#include "chat/chatchannel.h"
#include "container/containerciptypes.h"
#include "gamewindow/actionbuttoncontroller.h"
#include "gamewindow/assignactiondialogs.h"
#include "gamewindow/mousecursorcontroller.h"
#include "input/inputciptypes.h"
#include "input/gameactionhandlers.h"
#include "input/keyboardshortcuthandler.h"
#include "spells/spellinfostorage.h"
#include "spells/spellsciptypes.h"

#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>


#include <algorithm>
#include <tibia/appearances/appearanceinstance.h>

namespace tibia::gamewindow {

namespace {

using Setting = config::TActionSetting;

}

TActionBarManager::TActionBarManager(config::TClientOptions* clientOptions, game::TPremiumTimeStorage* premiumTimeStorage,
                                     spells::TSpellInfoStorage* spellInfoStorage, QObject* parent)
    : cip::tibia::gamewindow::TActionBarManager(parent)
    , m_clientOptions(clientOptions)
    , m_premiumTimeStorage(premiumTimeStorage)
    , m_spellInfoStorage(spellInfoStorage)
{
}

TActionBarManager::~TActionBarManager() = default;

void TActionBarManager::addGroup(std::shared_ptr<TActionBarGroup> group)
{
    const EActionBarPosition position = group->position();
    m_groups[position] = std::move(group);
}

void TActionBarManager::connectToGameActionRouter(const std::shared_ptr<game::TGameActionRouter>& router)
{
    game::TInternalGameActionRouter* internalRouter = router->internalRouter();
    for (const auto& [position, group] : m_groups)
        connect(group.get(), &TActionBarGroup::publishGameAction, internalRouter, &game::TInternalGameActionRouter::handleGameActionInternal);
}

void TActionBarManager::restoreLastAktionBarConfiguration()
{
    for (const auto& [position, group] : m_groups)
        group->restoreLastAktionBarConfiguration();
}

void TActionBarManager::handleGameAction(const input::IGameActionPtr& pGameAction)
{
    for (const auto& [position, group] : m_groups)
        group->handleGameAction(pGameAction);
}

void TActionBarManager::tick(const qint64 now)
{
    for (const auto& [position, group] : m_groups)
        group->tick(now);
}

void TActionBarManager::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    for (const auto& [position, group] : m_groups)
        group->connectToGameSession(GameSession);
    disconnect(m_unlockedSpellConnection);
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(GameSession.get());
    // A spell the server unlocks goes to a free action button.
    if (spells::TSpellsProtocolMessageHandler* spellsHandler = session ? session->spellsProtocolMessageHandler().get() : nullptr)
        m_unlockedSpellConnection = connect(spellsHandler, &spells::TSpellsProtocolMessageHandler::receivedUnlockedSpell, this,
                                            &TActionBarManager::onReceivedUnlockedSpell);
}

void TActionBarManager::onWorldEntered()
{
    if (const game::TGameSessionBase* session = tibia::game::currentGameSession(); session && session->hasLocalPlayer())
        config::THotkeyOptions::instance()->onCharacterLoggedIn();
    // The host's hotkey options load without announcing it.
    for (const auto& [position, group] : m_groups)
        for (TActionBarController* bar : group->bars()) {
            bar->onActionBarOptionsChanged();
            bar->onHotkeyOptionsChanged();
        }
}

void TActionBarManager::onReceivedUnlockedSpell(const int spellId)
{
    if (!m_clientOptions || !m_clientOptions->boolOption(config::EClientOption::actionButtonAutoInsertSpells))
        return;
    const spells::TSpellInfo* spell = m_spellInfoStorage ? m_spellInfoStorage->spell(spellId) : nullptr;
    if (!spell)
        return;
    const auto casts = [&](const Setting& setting) {
        const spells::TSpellInfo* cast = setting.kind == Setting::TEXT ? m_spellInfoStorage->spellOfText(setting.text) : nullptr;
        return cast && cast->id == spellId;
    };
    // Host: the hotkey options keep the current hotkey set the bars apply.
    for (const auto& [key, setting] : config::THotkeyOptions::instance()->currentSet().mappings) {
        if (casts(setting))
            return;
        for (const Setting& slot : setting.multiActions)
            if (casts(slot))
                return;
    }
    const bool newlyUnlocked = !spell->premium || (m_premiumTimeStorage && m_premiumTimeStorage->isPremium());
    for (const int actionBarId : { 1, 4, 7, 2, 5, 8, 3, 6, 9 }) {
        // The bar of the last unlocked group that has it.
        TActionBarController* bar = nullptr;
        for (const auto& [position, group] : m_groups)
            if (!group->locked())
                if (TActionBarController* found = group->managesBar(uint8_t(actionBarId)) ? group->bar(uint8_t(actionBarId)) : nullptr)
                    bar = found;
        if (bar && bar->insertSpell(spellId, newlyUnlocked))
            return;
    }
}

config::EUseType TActionBarManager::defaultUseType(const TAppearanceType* type)
{
    if (!type)
        return config::EUseType::Use;
    // A worn container (a backpack) is used like any other object.
    if (!type->isCloth() || type->isContainer())
        return type->isMultiUse() ? config::EUseType::SelectUseTarget : config::EUseType::Use;
    return config::EUseType::Equip;
}

}
