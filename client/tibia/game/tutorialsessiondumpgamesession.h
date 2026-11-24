#pragma once

#include "game/sessiondumpgamesession.h"
#include "game/tutorialsessiondumpgamesessionmetainfo.h"

#include <QObject>

#include <memory>

namespace tibia::effects {
class TEffectGameActionHandler;
}
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
    /// Built by the game client with the objects it shares with every session.
    TTutorialSessiondumpGameSession(std::shared_ptr<shared::TTibiaFileSystemHelper> tibiaFileSystemHelper,
                                    std::shared_ptr<appearances::TAppearancesManager> appearancesManager,
                                    std::shared_ptr<appearances::TAppearanceInstanceFactory> appearanceInstanceFactory,
                                    std::shared_ptr<config::TClientOptions> clientOptions,
                                    std::shared_ptr<shared::TTibiaClipboardHelper> clipboardHelper,
                                    std::shared_ptr<shared::TTibiaUrlTools> urlTools,
                                    std::shared_ptr<shared::IClockService> clientClockService,
                                    std::shared_ptr<analysers::TItemPriceStorage> itemPriceStorage,
                                    std::shared_ptr<sound::TSoundController> soundController,
                                    std::shared_ptr<input::TKeyboardShortcutHandler> keyboardShortcutHandler,
                                    spells::TSpellInfoStorage* spellInfoStorage, TPassiveAbilityInfoStorage* passiveAbilityInfoStorage,
                                    proficiency::TProficiencyStorage* proficiencyStorage,
                                    std::shared_ptr<gamewindow::TDialogStackController> dialogStackController = nullptr,
                                    QObject* parent = nullptr);
    ~TTutorialSessiondumpGameSession() override;

    /// The sessiondump session's setup (which loads the dump and its hints),
    /// then: no character configuration; the session's disconnected signal ends
    /// the tutorial mode (onDisconnected); the hints processor, the tutorial
    /// progress uploader and the tutorial's game action handler, each linked
    /// both ways with the game action router.
    void initialize(const std::shared_ptr<const TGameSessionMetaInfo>& metaInfo) override;
    /// Plays the dump, then publishes CHANGE_TUTORIAL_MODE_ENABLED (on).
    void startSession() override;
    /// Loads the entry's dump, then the entry's hints (a blocking GET of the
    /// hints URL) for the hints processor.
    void startSessiondump(const sessiondump::TSessiondumpCatalog::TEntry& entry) override;

protected:
    /// The base's game action providers and handlers, then the effect game
    /// action handler (the tutorial's effects on the map), fed by the router.
    void createGameActionProvidersAndHandlers() override;

public Q_SLOTS:
    /// Publishes CHANGE_TUTORIAL_MODE_ENABLED (off).
    virtual void onDisconnected();

protected Q_SLOTS:
    /// The tutorial ends with its dump: the session announces disconnected
    /// (the game client then connects the character's live session).
    void onSessiondumpFinished() override;
    /// The dump pauses at the next hint; the hints due at the dump's progress
    /// announce the player paused and are processed; with none due the player
    /// resumes. Then the sessiondump session's tick.
    void onSessiondumpTimerElapsed() override;
    /// The player's basic data arrived: the player's creature gets the
    /// tutorial character's name and its (male or female) tutorial outfit.
    void onPlayerDataBasicReceived();

private:
    std::shared_ptr<const TTutorialSessiondumpGameSessionMetaInfo> m_tutorialMetaInfo;
    std::shared_ptr<tutorial::TSessiondumpHintsProcessor> m_sessiondumpHintsProcessor;
    std::shared_ptr<tutorial::TSessiondumpHints> m_sessiondumpHints;
    std::shared_ptr<tutorial::TTutorialProgressUploader> m_tutorialProgressUploader;
    std::shared_ptr<tutorial::TTutorialSessiondumpGameActionHandler> m_tutorialSessiondumpGameActionHandler;
    std::shared_ptr<effects::TEffectGameActionHandler> m_effectGameActionHandler;
};

}
}
