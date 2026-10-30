#include "shared/sharedciptypes.h"

namespace tibia::shared {

namespace {
struct CipTypedKeep {
    TExperienceGainChangeTracker tExperienceGainChangeTracker;
    TIODeviceReader tIODeviceReader;
    TIODeviceWriter tIODeviceWriter;
    TImagestripResponse tImagestripResponse;
    TNullExperienceGainChangeTracker tNullExperienceGainChangeTracker;
    TNullExperiencePerHourProvider tNullExperiencePerHourProvider;
    TOptimized1PixelBorderImageProvider tOptimized1PixelBorderImageProvider;
    TTibiaClipboardHelper tTibiaClipboardHelper;
    TTibiaFileSystemHelper tTibiaFileSystemHelper;
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

} // namespace tibia::shared
