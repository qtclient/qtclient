#pragma once

#include "input/gameaction.h"

#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>
#include <vector>

namespace tibia::game {
class TGameActionRouter;
}

namespace tibia::app {

/// Host (no CipSoft counterpart): scripted smoke steps, so a smoke run never
/// needs input on the desktop. Once the world is entered (a login or a
/// replay), each --smoke-action publishes its game action through the game
/// client's action router, one after the other, each `delay` milliseconds
/// after the previous one. The action carries only its type (CipSoft's
/// TGameActionSimple); CONFIRM_TUTORIAL_HINT carries the tutorial hint the
/// router last showed (SHOW_TUTORIAL_HINT). With confirmHints every shown
/// tutorial hint is confirmed `confirmHintDelay` milliseconds after it shows.
class TSmokeActions final : public QObject
{
    Q_OBJECT
public:
    struct TStep
    {
        input::EGameAction action = input::EGameAction::UNKNOWN;
        int delayMs = 1000;
    };

    /// `<EGameAction name>[:<delay ms>]` per spec (names as in EGameAction,
    /// case-insensitive; default delay 1000). False with `error` on a bad spec.
    static bool parse(const QStringList& specs, std::vector<TStep>& steps, QString& error);

    TSmokeActions(std::shared_ptr<game::TGameActionRouter> router, std::vector<TStep> steps, bool confirmHints,
                  int confirmHintDelayMs, QObject* parent = nullptr);

    /// Starts the steps (call once the world is entered).
    void start();

private:
    void runStep(std::size_t index);
    void publish(input::IGameActionPtr action);
    void onRoutedAction(const input::IGameActionPtr& action);
    void confirmShownHint();

    std::shared_ptr<game::TGameActionRouter> m_router;
    std::vector<TStep> m_steps;
    bool m_confirmHints = false;
    int m_confirmHintDelayMs = 500;
    /// The tutorial hint last shown and not yet confirmed; 0 when none.
    int m_shownHintId = 0;
};

}
