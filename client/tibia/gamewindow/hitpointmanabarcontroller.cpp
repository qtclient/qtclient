#include "hitpointmanabarcontroller.h"

#include "config/clientoptions.h"
#include "game/gamesessionbase.h"
#include "game/playerdata.h"
#include "gamewindow/contextmenucontroller.h"

namespace tibia::gamewindow {

namespace {

using config::EClientOption;

void toggleOption(config::TClientOptions* ClientOptions, const EClientOption id)
{
    ClientOptions->setBoolOption(id, !ClientOptions->boolOption(id));
}

}

THitpointManabarController::THitpointManabarController(config::TClientOptions* ClientOptions, QObject* parent)
    : TSidebarPanelController(parent)
    , m_pClientOptions(ClientOptions)
{
    m_visible = true;
    if (!m_pClientOptions)
        return;
    connect(m_pClientOptions, &config::TClientOptions::statusPanelEnabledChanged, this,
            &THitpointManabarController::onOptionChanged);
    setVisible(m_pClientOptions->boolOption(EClientOption::statusPanelEnabled));
}

void THitpointManabarController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    m_gameSessionConnections.disconnectAll();
    m_pGameSession = GameSession;
    if (const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_pGameSession.get())) {
        if (const std::shared_ptr<game::TPlayerData>& playerData = session->playerData()) {
            m_gameSessionConnections.add(
                connect(playerData.get(), &game::TPlayerData::playerDataChanged, this, &THitpointManabarController::onPlayerDataChanged));
            onPlayerDataChanged();
        }
    }
    onOptionChanged();
}

void THitpointManabarController::onPlayerDataChanged()
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_pGameSession.get());
    if (!session || !session->playerData())
        return;
    const game::TPlayerData& playerData = *session->playerData();
    setHitpoints(playerData.health(), playerData.maxHealth());
    setMana(playerData.mana(), playerData.maxMana());
}

void THitpointManabarController::setHitpoints(const int current, const int max)
{
    if (m_currentHitpoints != current) {
        m_currentHitpoints = current;
        Q_EMIT currentHitpointsChanged();
    }
    if (m_maxHitpoints == max)
        return;
    m_maxHitpoints = max;
    Q_EMIT maxHitpointsChanged();
}

void THitpointManabarController::setMana(const int current, const int max)
{
    if (m_currentMana != current) {
        m_currentMana = current;
        Q_EMIT currentManaChanged();
    }
    if (m_maxMana == max)
        return;
    m_maxMana = max;
    Q_EMIT maxManaChanged();
}

void THitpointManabarController::setVisible(const bool visible)
{
    m_visible = visible;
    Q_EMIT visibilityChanged();
}

void THitpointManabarController::onOptionChanged()
{
    if (!m_pClientOptions)
        return;
    setVisible(m_pClientOptions->boolOption(EClientOption::statusPanelEnabled));
}

void THitpointManabarController::requestContextMenu()
{
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu || !m_pClientOptions)
        return;
    menu->beginCustomMenu();
    const auto addOption = [&](const char* label, const EClientOption id) {
        TContextMenuEntryItem* item = menu->addCustomEntry(qtTrId(label));
        // The option is read again when the entry is picked.
        connect(item, &TContextMenuEntryItem::triggered, this, [this, id] { toggleOption(m_pClientOptions, id); });
        item->setCheckable(true);
        item->setChecked(m_pClientOptions->boolOption(id));
    };
    addOption("optionsmenu_statusbar_enabled", EClientOption::statusBarEnabled);
    addOption("optionsmenu_statuspanel_enabled", EClientOption::statusPanelEnabled);
    menu->showCustomMenu();
}

}
