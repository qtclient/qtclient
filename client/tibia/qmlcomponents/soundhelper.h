#pragma once

#include <cip_gen.h>

namespace tibia::qmlcomponents {

/// QML singleton SoundHelper (CIP: TSoundHelperService, playSound 0x1010c4870).
/// Publishes SOUND_TRIGGER with the ESound id; the action handler plays via TSoundStorage.
class TSoundHelperService final : public cip::tibia::qmlcomponents::TSoundHelperService
{
    Q_OBJECT
public:
    explicit TSoundHelperService(QObject* parent = nullptr);

    Q_INVOKABLE void playSound(ESound Sound) override;
};

/// Installs the SOUND_TRIGGER game-action handler (CIP parameter = effect id).
void installSoundActionHandler();
/// Re-apply ClientOptions volumes to TSoundStorage (and the login anthem).
void applyOptionVolumes();

}
