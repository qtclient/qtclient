#pragma once

#include "game/sessiondumpgamesession.h"

#include <QObject>

#include <memory>

namespace tibia {
namespace game {

/// The tutorial a new character watches before its live session
/// (CIP game::TTutorialSessiondumpGameSession): a sessiondump session that turns
/// the tutorial mode on when it starts and off when it disconnects.
class TTutorialSessiondumpGameSession final : public TSessiondumpGameSession
{
    Q_OBJECT
public:
    explicit TTutorialSessiondumpGameSession(QObject* parent = nullptr);
    ~TTutorialSessiondumpGameSession() override;

    /// The sessiondump session's setup, then the session's disconnected signal
    /// ends the tutorial mode (onDisconnected).
    void initialize(const std::shared_ptr<const TGameSessionMetaInfo>& metaInfo) override;
    /// Plays the dump, then publishes CHANGE_TUTORIAL_MODE_ENABLED (on).
    void startSession() override;

public Q_SLOTS:
    /// Publishes CHANGE_TUTORIAL_MODE_ENABLED (off).
    virtual void onDisconnected();
};

}
}
