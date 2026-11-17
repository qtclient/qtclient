#include "skillwheelmanager.h"

#include "analysers/analyserstorages.h"
#include "input/gameactionhandlers.h"

#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/tgamenotify.h>

namespace tibia::skillwheel {

namespace {

using A = input::EGameAction;
using Args = std::vector<std::any>;

constexpr A Actions[] = { A::OPEN_OR_UPDATE_SKILL_WHEEL_DIALOG,
                          A::OPEN_SKILL_WHEEL_DIALOG_NO_WHEEL_MESSAGE,
                          A::OPEN_SKILL_WHEEL_DIALOG_PREVIEW_POSSIBLE_MESSAGE,
                          A::REQUEST_SKILL_WHEEL,
                          A::REQUEST_OWN_SKILL_WHEEL,
                          A::SAVE_SKILL_WHEEL,
                          A::GEM_ATELIER_SELECT_GEM,
                          A::FRAGMENT_WORKSHOP_SELECT_MOD };

}

TSkillWheelManager::TSkillWheelManager(QObject* parent)
    : QObject(parent)
    , m_storage(new TSkillWheelStorage(this))
    , m_dialog(new TSkillWheelDialogController(m_storage, this))
{
    // The own wheel's answer to a save does not open the dialog again.
    input::setActionHandler(A::OPEN_OR_UPDATE_SKILL_WHEEL_DIALOG, [this](const input::TGameAction&) {
        if (m_storage->isOwnCharacter() && m_skipNextOwnOpen) {
            clearSkipNextOwnOpen();
            return true;
        }
        m_dialog->showDialog();
        return true;
    });
    input::setActionHandler(A::OPEN_SKILL_WHEEL_DIALOG_NO_WHEEL_MESSAGE, [this](const input::TGameAction&) {
        m_dialog->showMessage(qtTrId("info"), qtTrId("skill_wheel_warning_no_skill_wheel").arg(RequiredLevel), false);
        return true;
    });
    input::setActionHandler(A::OPEN_SKILL_WHEEL_DIALOG_PREVIEW_POSSIBLE_MESSAGE, [this](const input::TGameAction&) {
        m_dialog->showMessage(qtTrId("info"),
                              qtTrId("skill_wheel_warning_no_skill_wheel").arg(RequiredLevel)
                                  + qtTrId("skill_wheel_warning_no_skill_wheel_show_preview_part").arg(qtTrId("ok")),
                              true);
        return true;
    });
    input::setActionHandler(A::REQUEST_SKILL_WHEEL, [this](const input::TGameAction& action) {
        m_actions.handleCreatureGameAction(action.creatureId);
        return true;
    });
    input::setActionHandler(A::REQUEST_OWN_SKILL_WHEEL, [this](const input::TGameAction&) {
        clearSkipNextOwnOpen();
        m_actions.handleRequestOnwSkillWheelGameAction();
        return true;
    });
    input::setActionHandler(A::SAVE_SKILL_WHEEL, [this](const input::TGameAction&) {
        saveWheel();
        return true;
    });
    input::setActionHandler(A::GEM_ATELIER_SELECT_GEM, [this](const input::TGameAction& action) {
        m_dialog->showDialog();
        m_dialog->openGemAtelier(0, action.parameter);
        return true;
    });
    input::setActionHandler(A::FRAGMENT_WORKSHOP_SELECT_MOD, [this](const input::TGameAction& action) {
        m_dialog->showDialog();
        m_dialog->openWorkshopMod(action.instance != 0, action.parameter);
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
    if (TSkillWheelProtocolMessageHandler* handler = TSkillWheelProtocolMessageHandler::liveInstance())
        handler->setSkillWheelStorage(m_storage);
    if (analysers::TAnalyserStorages* storages = analysers::TAnalyserStorages::instance()) {
        storages->addCharacterConfiguration(m_storage);
        connect(m_storage, &TSkillWheelStorage::configurationChangedFromUserInput, storages,
                &analysers::TAnalyserStorages::onConfigurationChanged);
    }

    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        clearSkipNextOwnOpen();
        m_dialog->clear();
        // CIP has one storage per game session.
        m_storage->clearServerData();
        m_storage->resetCurrentSkillWheel();
        m_storage->setGemInventory({});
        m_storage->setModGrades({}, {});
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

TSkillWheelManager::~TSkillWheelManager()
{
    if (TSkillWheelProtocolMessageHandler* handler = TSkillWheelProtocolMessageHandler::liveInstance())
        handler->setSkillWheelStorage(nullptr);
    if (analysers::TAnalyserStorages* storages = analysers::TAnalyserStorages::instance())
        storages->removeCharacterConfiguration(m_storage);
    for (const A type : Actions)
        input::setActionHandler(type, nullptr);
}

void TSkillWheelManager::clearSkipNextOwnOpen()
{
    m_skipNextOwnOpenTimer.stop();
    m_skipNextOwnOpen = false;
}

/// SAVE_SKILL_WHEEL carries no wheel here: the own edited wheel is saved.
void TSkillWheelManager::saveWheel()
{
    if (!m_storage->hasData() || !m_storage->isOwnCharacter() || m_storage->isPreview())
        return;
    m_actions.handleSaveSkillWheelGameAction(m_storage->currentWheel());
}

}
