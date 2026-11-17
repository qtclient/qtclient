#pragma once

#include <cip_gen.h>

namespace tibia {
namespace qmlcomponents {

/// QML singleton SoundHelper (CIP: TSoundHelperService, playSound).
/// Publishes SOUND_TRIGGER with the ESound id; TSoundController plays it.
class TSoundHelperService final : public cip::tibia::qmlcomponents::TSoundHelperService
{
    Q_OBJECT
public:
    explicit TSoundHelperService(QObject* parent = nullptr);

    Q_INVOKABLE void playSound(ESound Sound) override;

    /// The process' sound helper (CIP: the shared service the dialogs play
    /// their sounds through); the game client owns it and connects its
    /// publishGameAction. The QML singletons play through it.
    static TSoundHelperService* instance();
    static void setInstance(TSoundHelperService* service);
};

/// The pack weather audio follows the sound options.
void followSoundOptions();
/// Option change: the pack weather audio follows the ambience volume.
void applyOptionVolumes();

}
}
