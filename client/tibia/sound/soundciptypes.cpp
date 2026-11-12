#include "sound/soundciptypes.h"

namespace tibia::sound {

namespace {
struct CipTypedKeep {
    TSoundConcurrentException tSoundConcurrentException;
    TSoundControllerAmbienceObjectSound tSoundControllerAmbienceObjectSound;
    TSoundControllerCombatSound tSoundControllerCombatSound;
};
CipTypedKeep& typedKeep() { static CipTypedKeep keep; return keep; }
} // namespace

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)typedKeep();
    (void)parent;
}

} // namespace tibia::sound
