#include "deathdialogcontroller.h"


#include <QKeySequence>

#include <any>
#include <cstdint>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/tgamenotify.h>

namespace tibia::gamewindow {

namespace {

using Args = std::vector<std::any>;

}

TDeathDialogController::TDeathDialogController(QObject* parent)
    : QObject(parent)
    , m_box(new TMessageDialogController(this))
{
    m_box->setCloseOnButtonClicked(true);
    m_box->setTextCentered(true);
    m_box->setButtons({
        new TMessageDialogButton(QStringLiteral("Ok"), TMessageDialogController::Button::OK,
                                 QKeySequence(QStringLiteral("Return"))),
    });
    connect(m_box, &TMessageDialogController::acceptClicked, this, [this](bool) { finish(); });

    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::death, this,
            [this](int deathType, int penality) {
                present(static_cast<uint8_t>(deathType), static_cast<uint8_t>(penality));
            });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        close();
    });
}

TDeathDialogController::~TDeathDialogController() = default;

void TDeathDialogController::present(const uint8_t deathType, const uint8_t penality)
{
    Q_UNUSED(deathType);
    close();
    tibia::game::setSceneFrozen(true);

    QString message = QStringLiteral("You are dead.");
    // OT death packet omits penality when the feature is off; argInt then yields 0.
    if (penality > 0 && penality < 100)
        message += QStringLiteral("\n\nFair fight reduction: %1%.").arg(penality);
    message += QStringLiteral("\n\nClick Ok to leave this character.");

    m_box->showMessage(QStringLiteral("You are dead"), message);
}

void TDeathDialogController::finish()
{
    if (m_finishing)
        return;
    m_finishing = true;
    close();
    tibia::game::setSceneFrozen(false);
    if (tibia::game::isOnline())
        tibia::game::safeLogout();
    m_finishing = false;
}

void TDeathDialogController::close()
{
    if (m_box && m_box->dialog()->isInStack())
        m_box->dialog()->close();
}

}
