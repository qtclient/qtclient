#include "shared/sharedciptypes.h"

#include "client/gamecore.h"
#include "network/gameserverconnection.h"

#include <QClipboard>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QRandomGenerator>

#include <numeric>

namespace tibia::shared {

namespace {
struct CipTypedKeep {
    TExperienceGainChangeTracker tExperienceGainChangeTracker;
    TImagestripResponse tImagestripResponse;
    TNullExperienceGainChangeTracker tNullExperienceGainChangeTracker;
    TNullExperiencePerHourProvider tNullExperiencePerHourProvider;
    TTibiaClipboardHelper tTibiaClipboardHelper;
    TClockService tClockService;
    TCoordinate tCoordinate;
};
CipTypedKeep& typedKeep() { static CipTypedKeep keep; return keep; }
} // namespace

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    Q_UNUSED(parent);
    (void)typedKeep();
}

} // namespace tibia::shared
