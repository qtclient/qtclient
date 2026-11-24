#pragma once

#include <cip_gen.h>

#include <QString>

#include <memory>

namespace tibia::creatures {
class TCreature;
}
namespace tibia::shared {
class IClockService;
}

namespace tibia::game {

class TGameSessionMetaInfo;

/// CIP game::IGameSession: a game session (live, replayed or the tutorial) as
/// the game client and its controllers see it. The client builds one per
/// session from a TGameSessionMetaInfo, initializes it, makes it the current
/// session and releases it when it disconnects; the next login gets a new one.
class IGameSession : public cip::tibia::game::IGameSession
{
    Q_OBJECT
public:
    explicit IGameSession(QObject* parent = nullptr);
    ~IGameSession() override;

    /// Builds the session's storages, handlers and providers for `metaInfo`;
    /// throws std::invalid_argument for meta info of another kind of session.
    virtual void initialize(const std::shared_ptr<const TGameSessionMetaInfo>& metaInfo) = 0;
    /// Starts the session once it is the client's current one: a replay starts
    /// playing; a live session is already connecting.
    virtual void startSession() = 0;
    /// Ends the session's connection or playback now; the session then
    /// reports disconnected.
    virtual void disconnectNow() = 0;
    /// CIP v51: the session reads and handles what arrived from the network
    /// (the game client's network timer calls it every 50 ms). The base has none.
    virtual void processNetwork() { }
    /// The clock the session's frames run on: the next frame's predicted
    /// time is read from it.
    virtual std::shared_ptr<shared::IClockService> clockService() const = 0;

Q_SIGNALS:
    void playerCreatureAddedToCreatureStorage(std::weak_ptr<tibia::creatures::TCreature> pPlayer);
    /// CIP's ESessionEndInformation.
    void sessionEndInformation(int Reason);
    /// CIP's ELoginErrorAction.
    void gameLoginErrorMessage(const QString& ErrorMessage, int Action);
    /// CIP's TSeconds.
    void gameLoginWaitMessage(const QString& WaitMessage, int WaitSeconds);
};

}
