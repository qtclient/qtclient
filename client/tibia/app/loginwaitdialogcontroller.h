#pragma once

#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

#include <QTimer>

namespace tibia {
namespace app {

/// Host: WaitDialog.qml as an indeterminate please-wait while the login
/// request and the world connection are in flight (CipSoft's TWaitDialogController
/// only counts down a fixed duration).
class TLoginWaitDialogController final : public gamewindow::TDialogController<cip::tibia::gamewindow::TWaitDialogController>
{
    Q_OBJECT
public:
    explicit TLoginWaitDialogController(QObject* parent = nullptr);

    /// loginwait_*: "Your character list is being loaded. Please wait."
    void showCharacterList();
    /// connecting_*: "Connecting to the game world. Please wait."
    void showGameWorld();
    void close();

    /// Which wait abortWait() just dismissed. close() clears the live phase.
    enum class Phase { None, CharacterList, GameWorld };
    Phase abortedPhase() const { return m_abortedPhase; }

    Q_INVOKABLE void abortWait() override;

Q_SIGNALS:
    void aborted();

private:
    void tick();

    void present(const char* captionId, const char* textId, Phase phase);

    QTimer m_tick;
    int m_elapsed = 0;
    Phase m_phase = Phase::None;
    Phase m_abortedPhase = Phase::None;
};

}
}
