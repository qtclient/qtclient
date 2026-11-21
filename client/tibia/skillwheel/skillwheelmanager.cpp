#include "skillwheelmanager.h"

#include "input/gameactionhandlers.h"

#include <tibia/game/gamesessionbase.h>

#include <tibia/game/gamenotify.h>

#include <any>
#include <vector>

namespace tibia::skillwheel {

namespace {

using A = input::EGameAction;
using Args = std::vector<std::any>;

constexpr A Actions[] = { A::OPEN_OR_UPDATE_SKILL_WHEEL_DIALOG,
                          A::OPEN_SKILL_WHEEL_DIALOG_NO_WHEEL_MESSAGE,
                          A::OPEN_SKILL_WHEEL_DIALOG_PREVIEW_POSSIBLE_MESSAGE,
                          A::REQUEST_OWN_SKILL_WHEEL,
                          A::GEM_ATELIER_SELECT_GEM,
                          A::FRAGMENT_WORKSHOP_SELECT_MOD };

}

TSkillWheelManager::TSkillWheelManager(QObject* parent)
    : QObject(parent)
    , m_storage(new TSkillWheelStorage(this))
    , m_dialog(new TSkillWheelDialogController(m_storage, this))
{
    // The own wheel's answer to a save does not open the dialog again.
    m_gameActions.on(A::OPEN_OR_UPDATE_SKILL_WHEEL_DIALOG, [this](const input::IGameAction&) {
        if (m_storage->isOwnCharacter() && m_skipNextOwnOpen) {
            clearSkipNextOwnOpen();
            return true;
        }
        const auto tab = m_dialog->dialogTab();
        m_dialog->openDialogTab(tab == TSkillWheelDialogController::NoTabInitialState ? TSkillWheelDialogController::Wheel : tab);
        return true;
    });
    m_gameActions.on(A::OPEN_SKILL_WHEEL_DIALOG_NO_WHEEL_MESSAGE, [this](const input::IGameAction&) {
        m_dialog->showMessage(qtTrId("info"), qtTrId("skill_wheel_warning_no_skill_wheel").arg(RequiredLevel), false);
        return true;
    });
    m_gameActions.on(A::OPEN_SKILL_WHEEL_DIALOG_PREVIEW_POSSIBLE_MESSAGE, [this](const input::IGameAction&) {
        m_dialog->showMessage(qtTrId("info"),
                              qtTrId("skill_wheel_warning_no_skill_wheel").arg(RequiredLevel)
                                  + qtTrId("skill_wheel_warning_no_skill_wheel_show_preview_part").arg(qtTrId("ok")),
                              true);
        return true;
    });
    // The session's game action handler asks the server for the own wheel.
    m_gameActions.on(A::REQUEST_OWN_SKILL_WHEEL, [this](const input::IGameAction&) {
        clearSkipNextOwnOpen();
        return false;
    });
    m_gameActions.on(A::GEM_ATELIER_SELECT_GEM, [this](const input::gameactions::TSelectAtelierGemGameAction& action) {
        m_dialog->openDialogTab(TSkillWheelDialogController::GemAtelier);
        m_dialog->openGemAtelier(0, action.gemId());
        return true;
    });
    m_gameActions.on(A::FRAGMENT_WORKSHOP_SELECT_MOD, [this](const input::gameactions::TSelectFragmentWorkshopModGameAction& action) {
        m_dialog->openDialogTab(TSkillWheelDialogController::FragmentWorkshop);
        m_dialog->openWorkshopMod(action.supremeMod(), action.modId());
        return true;
    });

    m_skipNextOwnOpenTimer.setInterval(1000);
    m_skipNextOwnOpenTimer.setSingleShot(true);
    connect(&m_skipNextOwnOpenTimer, &QTimer::timeout, this, [this] { m_skipNextOwnOpen = false; });
    if (auto* page = qobject_cast<TSkillWheelPageController*>(m_dialog->wheelPageController()))
        connect(page, &TSkillWheelPageController::preapareToSkipNextOpenForOwnPlayer, this, [this] {
            m_skipNextOwnOpen = true;
            m_skipNextOwnOpenTimer.start();
        });
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

TSkillWheelManager::~TSkillWheelManager() = default;

void TSkillWheelManager::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    clearSkipNextOwnOpen();
    m_dialog->clear();
    m_storage->clearServerData();
    m_storage->resetCurrentSkillWheel();
    m_storage->setGemInventory({});
    m_storage->setModGrades({}, {});
    if (auto* session = dynamic_cast<game::TGameSessionBase*>(GameSession.get()))
        session->skillWheelProtocolMessageHandler()->setSkillWheelStorage(m_storage);
}

void TSkillWheelManager::clearSkipNextOwnOpen()
{
    m_skipNextOwnOpenTimer.stop();
    m_skipNextOwnOpen = false;
}


}
