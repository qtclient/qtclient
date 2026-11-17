#include "tutorial/tutorialsessiondumpgameactionhandler.h"

#include "tutorial/tutorialprogressuploader.h"

namespace tibia::tutorial {

namespace {

input::IGameActionPtr switchClientOptions(const int mode) { return std::make_shared<input::TGameActionSwitchClientOptions>(mode); }

input::IGameActionPtr changeContextMenuEnabled(const bool enabled)
{
    return std::make_shared<input::TGameActionChangeContextMenuEnabled>(enabled);
}

}

TTutorialSessiondumpGameActionHandler::TTutorialSessiondumpGameActionHandler(std::shared_ptr<TTutorialProgressUploader> uploader,
                                                                             const QString& characterName)
    : m_uploader(std::move(uploader))
    , m_characterName(characterName)
{
}

TTutorialSessiondumpGameActionHandler::~TTutorialSessiondumpGameActionHandler() = default;

void TTutorialSessiondumpGameActionHandler::handleGameAction(const input::IGameActionPtr pGameAction)
{
    if (const auto* progress = input::gameActionAs<input::TGameActionTutorialProgress>(pGameAction)) {
        m_uploader->uploadProgress(progress->tutorialProgress());
        if (progress->tutorialProgress() == QLatin1String("TutorialEnd"))
            Q_EMIT publishGameAction(std::make_shared<input::TGameActionFinishedTutorialForCharacter>(m_characterName));
        return;
    }
    const auto* mode = input::gameActionAs<input::TGameActionChangeTutorialModeEnabled>(pGameAction);
    if (!mode)
        return;
    if (!mode->enabled()) {
        Q_EMIT publishGameAction(changeContextMenuEnabled(true));
        Q_EMIT publishGameAction(switchClientOptions(0));
    } else {
        Q_EMIT publishGameAction(switchClientOptions(1));
        Q_EMIT publishGameAction(changeContextMenuEnabled(false));
    }
}

}
