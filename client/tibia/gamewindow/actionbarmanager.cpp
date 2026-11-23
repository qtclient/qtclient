#include "actionbarmanager.h"
#include "game/gamesessionbase.h"
#include "input/keyboardshortcuthandler.h"

#include "app/clientoptions.h"
#include "config/clientoptions.h"
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

#include <QQuickItem>
#include <QQuickWindow>

#include <algorithm>
#include <tibia/game/gamesession.h>
#include <tibia/game/thingsession_forward.h>

namespace tibia::gamewindow {

namespace {

using Setting = config::TActionSetting;
using A = input::EGameAction;
using P = EActionBarPosition;

}

TActionBarManager::TActionBarManager(QQuickItem* gameWindowItem, QQuickItem* mapWindowPane, QQmlEngine* engine,
                                     chat::TChatModeManager* chatModeManager, QObject* parent)
    : cip::tibia::gamewindow::TActionBarManager(parent)
    , m_gameWindowItem(gameWindowItem)
    , m_targetSelection(new TTargetSelectionController(gameWindowItem, this))
{
    connect(m_targetSelection, &TTargetSelectionController::publishGameAction, this, &TActionBarManager::publishGameAction);
    if (gameWindowItem) {
        const std::array<std::pair<P, const char*>, 3> containers{ { { P::ActionBarPositionBottom, "bottomActionBars" },
                                                                    { P::ActionBarPositionLeft, "leftActionBars" },
                                                                    { P::ActionBarPositionRight, "rightActionBars" } } };
        for (const auto& [position, name] : containers) {
            auto* container = gameWindowItem->findChild<QQuickItem*>(QLatin1String(name));
            addGroup(std::make_unique<TActionBarGroup>(position, container, engine, gameWindowItem, mapWindowPane,
                                                       config::TClientOptions::instance(), chatModeManager, this));
        }
        restoreLastAktionBarConfiguration();
        // Host: the frame the groups tick on.
        const auto connectFrame = [this](QQuickWindow* window) {
            disconnect(m_frameConnection);
            if (window)
                m_frameConnection = connect(window, &QQuickWindow::afterAnimating, this, [this] { tick(game::TCooldownStorage::now()); });
        };
        connect(gameWindowItem, &QQuickItem::windowChanged, this, connectFrame);
        connectFrame(gameWindowItem->window());
    }
}

TActionBarManager::~TActionBarManager()
{
    disconnect(m_frameConnection);
}

void TActionBarManager::addGroup(std::unique_ptr<TActionBarGroup> group)
{
    connect(group.get(), &TActionBarGroup::publishGameAction, this, &TActionBarManager::publishGameAction);
    const EActionBarPosition position = group->position();
    m_groups[position] = std::move(group);
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
    if (m_targetSelection)
        m_targetSelection->handleGameAction(pGameAction);
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

TActionBarController* TActionBarManager::bar(const int actionBarId) const
{
    for (const auto& [position, group] : m_groups)
        if (TActionBarController* found = group->managesBar(uint8_t(actionBarId)) ? group->bar(uint8_t(actionBarId)) : nullptr)
            return found;
    return nullptr;
}

void TActionBarManager::onWorldEntered()
{
    if (tibia::game::hasLocalPlayer()) {
        config::THotkeyOptions::instance()->onCharacterLoggedIn();
        auto action = std::make_shared<input::TGameActionTryToSwitchHotkeySet>(QString::fromStdString(tibia::game::characterName()), false);
        Q_EMIT publishGameAction(action);
    }
    // The host's hotkey options load without announcing it.
    for (const auto& [position, group] : m_groups)
        for (TActionBarController* bar : group->bars()) {
            bar->onActionBarOptionsChanged();
            bar->onHotkeyOptionsChanged();
        }
}

void TActionBarManager::onReceivedUnlockedSpell(const int spellId)
{
    const QJsonObject options = app::ClientOptions::load().options();
    if (!options.value(QLatin1String("actionButtonAutoInsertSpells")).toBool(true))
        return;
    spells::TSpellInfoStorage* storage = spells::TSpellInfoStorage::instance();
    const spells::TSpellInfo* spell = storage->spell(spellId);
    if (!spell)
        return;
    const auto casts = [&](const Setting& setting) {
        const spells::TSpellInfo* cast = setting.kind == Setting::TEXT ? storage->spellOfText(setting.text) : nullptr;
        return cast && cast->id == spellId;
    };
    for (const auto& [key, setting] : config::THotkeyOptions::instance()->currentSet().mappings) {
        if (casts(setting))
            return;
        for (const Setting& slot : setting.multiActions)
            if (casts(slot))
                return;
    }
    const bool newlyUnlocked = !spell->premium
                               || (tibia::game::hasLocalPlayer() && tibia::game::localPlayerIsPremium());
    for (const int actionBarId : { 1, 4, 7, 2, 5, 8, 3, 6, 9 }) {
        for (const auto& [position, group] : m_groups) {
            if (group->locked())
                continue;
            for (TActionBarController* bar : group->bars())
                if (bar->actionBarId() == actionBarId && bar->insertSpell(spellId, newlyUnlocked))
                    return;
        }
    }
}

config::EUseType TActionBarManager::defaultUseType(const int objectId)
{
    const TAppearanceTypePtr type = objectId > 0 ? tibia::game::thingType(uint16_t(objectId), ThingCategoryItem) : nullptr;
    if (!type)
        return config::EUseType::Use;
    if ((type->isCloth() || type->getClothSlot() > 0) && !type->isMultiUse())
        return config::EUseType::Equip;
    return type->isMultiUse() ? config::EUseType::SelectUseTarget : config::EUseType::Use;
}

}
