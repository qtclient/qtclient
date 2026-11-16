#pragma once

// CIP-named shared surfaces (promoted from cipwire).

#include "shared/cip_typed.h"
#include "clockservice.h"
#include "coordinate.h"

#include <cip_gen.h>

#include <QAuthenticator>
#include <QByteArray>
#include <QElapsedTimer>
#include <QNetworkReply>
#include <QObject>

#include <string_view>
#include <QString>
#include <QtGlobal>
#include <QUrl>

#include <array>
#include <cstdint>
#include <vector>

namespace tibia {
namespace shared {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);

class TDynamicResourcesHelper final : public cip::tibia::shared::TDynamicResourcesHelper
{
    Q_OBJECT
public:
    using cip::tibia::shared::TDynamicResourcesHelper::TDynamicResourcesHelper;
};

class TExperienceGainChangeTracker final : public ::tibia::CipTyped {
public:
    TExperienceGainChangeTracker() = default;
};

class TImagestripResponse final : public ::tibia::CipTyped {
public:
    TImagestripResponse() = default;
};

class TNullExperienceGainChangeTracker final : public ::tibia::CipTyped {
public:
    TNullExperienceGainChangeTracker() = default;
};

class TNullExperiencePerHourProvider final : public ::tibia::CipTyped {
public:
    TNullExperiencePerHourProvider() = default;
};

// TOptimized1PixelBorderImageProvider — live QQuickImageProvider in
// imageproviders/optimizedborderimageprovider.h (CipSoft requestImage).

class TTibiaClipboardHelper final : public ::tibia::CipTyped {
public:
    TTibiaClipboardHelper() = default;
};

}
}


