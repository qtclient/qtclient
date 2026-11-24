#include "skillwheelmanager.h"

#include "input/gameactionhandlers.h"

#include <tibia/game/gamesessionbase.h>

#include <tibia/game/gamenotify.h>

#include <any>
#include <vector>

namespace tibia::skillwheel {

namespace {

using Args = std::vector<std::any>;


}

TSkillWheelManager::TSkillWheelManager(QObject* parent)
    : QObject(parent)
    , m_storage(new TSkillWheelStorage(this))
    , m_dialog(new TSkillWheelDialogController(m_storage, this))
{
    // SwitchPreset: the preset at the position (from 1) in name order.
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onPreset", [this](GameObject*, const Args& args) {
        const auto id = static_cast<size_t>(client::gamecore::argInt(args, 0));
        const std::map<QString, TWheel> presets = m_storage->presets();
        if (id == 0 || id > presets.size())
            return std::any{};
        if (auto* page = qobject_cast<TSkillWheelPageController*>(m_dialog->wheelPageController()))
            page->selectPresetByName(std::next(presets.begin(), static_cast<std::ptrdiff_t>(id - 1))->first, false);
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onPremiumChange", [this](GameObject*, const Args&) {
        m_dialog->onPremiumChanged();
        return std::any{};
    });
}

// The dialog (its pages hold the storage) goes before the storage.
TSkillWheelManager::~TSkillWheelManager() { delete m_dialog; }

void TSkillWheelManager::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    m_dialog->connectToGameSession();
    m_storage->clearServerData();
    m_storage->resetCurrentSkillWheel();
    m_storage->setGemInventory({});
    m_storage->setModGrades({}, {});
    if (auto* session = dynamic_cast<game::TGameSessionBase*>(GameSession.get()))
        session->skillWheelProtocolMessageHandler()->setSkillWheelStorage(m_storage);
}


}
