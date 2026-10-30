#pragma once

#include "client/gamecore.h"
#include "gamewindow/messagedialogcontroller.h"

#include <QObject>

#include <vector>

namespace tibia::gamewindow {

/// Stock "You are dead" dialog. OT servers deliver death via opcode 0x28 →
/// Game::processDeath → g_game.onDeath. CIP's TDeathDialogController /
/// GameserverMessageDead path is not wired yet (types skipped in cipgen).
class TDeathDialogController final : public QObject
{
    Q_OBJECT
public:
    explicit TDeathDialogController(QObject* parent = nullptr);
    ~TDeathDialogController() override;

private:
    void present(uint8_t deathType, uint8_t penality);
    void finish();
    void close();

    TMessageDialogController* m_box = nullptr;
    std::vector<client::gamecore::Hook> m_hooks;
    bool m_finishing = false;
};

}
