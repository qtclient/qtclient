#pragma once

#include "game/sessiondumpgamesession.h"
#include "game/tutorialsessiondumpgamesessionmetainfo.h"

#include <QObject>

#include <memory>

namespace tibia::tutorial {
class TSessiondumpHints;
class TSessiondumpHintsProcessor;
class TTutorialProgressUploader;
class TTutorialSessiondumpGameActionHandler;
}

namespace tibia {
namespace game {

/// The tutorial a new character watches before its live session
/// (CIP game::TTutorialSessiondumpGameSession): a sessiondump session that turns
/// the tutorial mode on when it starts and off when it disconnects, plays the
/// tutorial's hints along the dump and uploads the character's tutorial progress.
class TTutorialSessiondumpGameSession final : public TSessiondumpGameSession
{
    Q_OBJECT
public:
    explicit TTutorialSessiondumpGameSession(QObject* parent = nullptr);
    ~TTutorialSessiondumpGameSession() override;

    /// The sessiondump session's setup, then: the session's disconnected signal
    /// ends the tutorial mode (onDisconnected); the tutorial progress uploader
    /// and the tutorial's game action handler, linked both ways with the game
    /// action router.
    void initialize(const std::shared_ptr<const TGameSessionMetaInfo>& metaInfo) override;
    /// Plays the dump, then publishes CHANGE_TUTORIAL_MODE_ENABLED (on).
    void startSession() override;
    /// Plays the entry's dump, then loads the entry's hints (a blocking GET of
    /// the hints URL) for the hints processor.
    void startSessiondump(const sessiondump::TSessiondumpCatalog::TEntry& entry) override;

public Q_SLOTS:
    /// Publishes CHANGE_TUTORIAL_MODE_ENABLED (off).
    virtual void onDisconnected();

protected Q_SLOTS:
    /// The dump pauses at the next hint; the hints due at the dump's progress
    /// announce the player paused and are processed; with none due the player
    /// resumes. Then the sessiondump session's tick.
    void onSessiondumpTimerElapsed() override;

private:
    std::shared_ptr<const TTutorialSessiondumpGameSessionMetaInfo> m_tutorialMetaInfo;
    std::shared_ptr<tutorial::TSessiondumpHintsProcessor> m_sessiondumpHintsProcessor;
    std::shared_ptr<tutorial::TSessiondumpHints> m_sessiondumpHints;
    std::shared_ptr<tutorial::TTutorialProgressUploader> m_tutorialProgressUploader;
    std::shared_ptr<tutorial::TTutorialSessiondumpGameActionHandler> m_tutorialSessiondumpGameActionHandler;
};

}
}
