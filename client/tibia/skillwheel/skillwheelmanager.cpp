#include "skillwheelmanager.h"

#include "input/gameactionhandlers.h"

#include <array>
#include <limits>
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

void publish(const A type) { input::publish(input::makeAction(type)); }

bool hasVocation()
{
    if (!tibia::game::localPlayer())
        return false;
    const int profession = tibia::game::localPlayerVocation() % 10;
    return profession >= 1 && profession <= VocationCount;
}

}

TSkillWheelManager::TSkillWheelManager(QObject* parent)
    : QObject(parent)
    , m_storage(new TSkillWheelStorage(this))
    , m_dialog(new TSkillWheelDialogController(m_storage, this))
{
    input::setActionHandler(A::OPEN_OR_UPDATE_SKILL_WHEEL_DIALOG, [this](const input::TGameAction&) {
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
        if (tibia::game::localPlayer() && action.creatureId == tibia::game::localPlayerId())
            requestOwnWheel();
        else if (action.creatureId != 0)
            tibia::game::openWheel(action.creatureId);
        return true;
    });
    input::setActionHandler(A::REQUEST_OWN_SKILL_WHEEL, [this](const input::TGameAction&) {
        clearSkipNextOwnOpen();
        requestOwnWheel();
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
    connect(m_storage, &TSkillWheelStorage::serverDataReceived, this, &TSkillWheelManager::onServerData);

    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        clearSkipNextOwnOpen();
        m_dialog->clear();
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onPremiumChange", [this](GameObject*, const Args&) {
        m_dialog->onPremiumChanged();
        return std::any{};
    });
}

TSkillWheelManager::~TSkillWheelManager()
{
    for (const A type : Actions)
        input::setActionHandler(type, nullptr);
}

/// CIP : from level 51 the server is asked when the player data
/// says Premium or the login session does not; otherwise a vocation offers
/// the preview (308), none only explains (307).
void TSkillWheelManager::requestOwnWheel()
{
    if (!tibia::game::localPlayer() || tibia::game::localPlayerId() == 0)
        return;
    if (tibia::game::localPlayerLevel() >= RequiredLevel
        && (tibia::game::localPlayerIsPremium() || !m_sessionPremium)) {
        tibia::game::openWheel(tibia::game::localPlayerId());
        return;
    }
    publish(hasVocation() ? A::OPEN_SKILL_WHEEL_DIALOG_PREVIEW_POSSIBLE_MESSAGE
                          : A::OPEN_SKILL_WHEEL_DIALOG_NO_WHEEL_MESSAGE);
}

/// CIP : a wheel message without slices publishes 308 / 307.
void TSkillWheelManager::onServerData(const bool canView)
{
    if (!canView) {
        publish(tibia::game::localPlayer() && hasVocation() ? A::OPEN_SKILL_WHEEL_DIALOG_PREVIEW_POSSIBLE_MESSAGE
                                                            : A::OPEN_SKILL_WHEEL_DIALOG_NO_WHEEL_MESSAGE);
        return;
    }
    if (m_storage->isLocalPlayer() && m_skipNextOwnOpen) {
        clearSkipNextOwnOpen();
        return;
    }
    publish(A::OPEN_OR_UPDATE_SKILL_WHEEL_DIALOG);
}

void TSkillWheelManager::clearSkipNextOwnOpen()
{
    m_skipNextOwnOpenTimer.stop();
    m_skipNextOwnOpen = false;
}

/// CIP SAVE_SKILL_WHEEL: the 36 slices and eight vessel gem ids (0xffff empty).
void TSkillWheelManager::saveWheel()
{
    if (!m_storage->hasData() || !m_storage->isLocalPlayer() || m_storage->isPreview())
        return;
    const TWheel& wheel = m_storage->currentWheel();
    std::vector<uint16_t> points(wheel.points.begin(), wheel.points.end());
    std::array<uint16_t, VesselCount> gems{};
    for (int v = 0; v < VesselCount; ++v)
        gems[v] = wheel.gems[v] == NoGem ? std::numeric_limits<uint16_t>::max() : static_cast<uint16_t>(wheel.gems[v]);
    tibia::game::sendApplyWheelPoints(points, gems);
}

}
