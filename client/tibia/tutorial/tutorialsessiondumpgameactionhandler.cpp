#include "tutorial/tutorialsessiondumpgameactionhandler.h"

#include "tutorial/tutorialprogressuploader.h"

namespace tibia::tutorial {

namespace {

input::TGameActionPtr switchClientOptions(const int mode)
{
    auto action = std::make_shared<input::TGameActionSwitchClientOptions>();
    action->parameter = mode;
    return action;
}

input::TGameActionPtr changeContextMenuEnabled(const bool enabled)
{
    auto action = std::make_shared<input::TGameActionChangeContextMenuEnabled>();
    action->enable = enabled;
    return action;
}

}

TTutorialSessiondumpGameActionHandler::TTutorialSessiondumpGameActionHandler(std::shared_ptr<TTutorialProgressUploader> uploader,
                                                                             const QString& characterName)
    : m_uploader(std::move(uploader))
    , m_characterName(characterName)
{
}

TTutorialSessiondumpGameActionHandler::~TTutorialSessiondumpGameActionHandler() = default;

void TTutorialSessiondumpGameActionHandler::handleGameAction(const input::TGameActionPtr pGameAction)
{
    if (pGameAction->type == input::EGameAction::TUTORIAL_PROGRESS) {
        m_uploader->uploadProgress(pGameAction->text);
        if (pGameAction->text == QLatin1String("TutorialEnd")) {
            auto finished = std::make_shared<input::TGameActionFinishedTutorialForCharacter>();
            finished->text = m_characterName;
            Q_EMIT publishGameAction(finished);
        }
        return;
    }
    if (pGameAction->type != input::EGameAction::CHANGE_TUTORIAL_MODE_ENABLED)
        return;
    if (!pGameAction->enable) {
        Q_EMIT publishGameAction(changeContextMenuEnabled(true));
        Q_EMIT publishGameAction(switchClientOptions(0));
    } else {
        Q_EMIT publishGameAction(switchClientOptions(1));
        Q_EMIT publishGameAction(changeContextMenuEnabled(false));
    }
}

}
