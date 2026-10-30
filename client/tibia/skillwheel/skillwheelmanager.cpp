#include "skillwheelmanager.h"

#include "input/gameactionhandlers.h"

#include <tibia/world/game.h>
#include <tibia/world/localplayer.h>

#include <limits>

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

bool hasVocation(const LocalPlayerPtr& player)
{
    const int profession = player->getVocation() % 10;
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
        const LocalPlayerPtr& player = g_game.getLocalPlayer();
        if (player && action.creatureId == player->getId())
            requestOwnWheel();
        else if (action.creatureId != 0)
            g_game.openWheel(action.creatureId);
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

    m_hooks.emplace_back("g_game", "onGameEnd", [this](LuaObject*, const Args&) {
        clearSkipNextOwnOpen();
        m_dialog->clear();
        return std::any{};
    });
    m_hooks.emplace_back("object", "onPremiumChange", [this](LuaObject* self, const Args&) {
        if (self == g_game.getLocalPlayer().get())
            m_dialog->onPremiumChanged();
        return std::any{};
    });
}

TSkillWheelManager::~TSkillWheelManager()
{
    for (const A type : Actions)
        input::setActionHandler(type, nullptr);
}

/// CIP 0x100f9fe10: from level 51 the server is asked when the player data
/// says Premium or the login session does not; otherwise a vocation offers
/// the preview (308), none only explains (307).
void TSkillWheelManager::requestOwnWheel()
{
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    if (!player || player->getId() == 0)
        return;
    if (player->getLevel() >= RequiredLevel && (player->isPremium() || !m_sessionPremium)) {
        g_game.openWheel(player->getId());
        return;
    }
    publish(hasVocation(player) ? A::OPEN_SKILL_WHEEL_DIALOG_PREVIEW_POSSIBLE_MESSAGE : A::OPEN_SKILL_WHEEL_DIALOG_NO_WHEEL_MESSAGE);
}

/// CIP 0x100fa07a0: a wheel message without slices publishes 308 / 307.
void TSkillWheelManager::onServerData(const bool canView)
{
    if (!canView) {
        const LocalPlayerPtr& player = g_game.getLocalPlayer();
        publish(player && hasVocation(player) ? A::OPEN_SKILL_WHEEL_DIALOG_PREVIEW_POSSIBLE_MESSAGE : A::OPEN_SKILL_WHEEL_DIALOG_NO_WHEEL_MESSAGE);
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

/// CIP SAVE_SKILL_WHEEL: the 36 slices and the vessel gems by domain
/// (green, red, acqua, purple; 0xffff empty).
void TSkillWheelManager::saveWheel()
{
    if (!m_storage->hasData() || !m_storage->isLocalPlayer() || m_storage->isPreview())
        return;
    const TWheel& wheel = m_storage->currentWheel();
    std::vector<uint16_t> points(wheel.points.begin(), wheel.points.end());
    uint16_t gems[DomainCount];
    for (int d = 0; d < DomainCount; ++d)
        gems[d] = wheel.gems[d] == NoGem ? std::numeric_limits<uint16_t>::max() : static_cast<uint16_t>(wheel.gems[d]);
    g_game.sendApplyWheelPoints(points, gems[0], gems[1], gems[2], gems[3]);
}

}
