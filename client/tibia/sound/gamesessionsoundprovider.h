#pragma once

#include <cip_gen.h>

#include <QMetaObject>

#include <memory>

namespace tibia {
namespace sound {

class TSoundStorage;

/// CIP sound::TGameSessionSoundProvider: a game session's sound storage for
/// the client's sound controller.
class TGameSessionSoundProvider final : public cip::tibia::sound::TGameSessionSoundProvider
{
    Q_OBJECT
public:
    explicit TGameSessionSoundProvider(std::shared_ptr<TSoundStorage> soundStorage);
    ~TGameSessionSoundProvider() override;

    const std::shared_ptr<TSoundStorage>& soundStorage() const { return m_soundStorage; }

public Q_SLOTS:
    void onSoundsToPlayChanged();

private:
    std::shared_ptr<TSoundStorage> m_soundStorage;
    QMetaObject::Connection m_connection;
};

}
}
