#pragma once

// CIP-named shared surfaces (promoted from cipwire).

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

class TDynamicResourcesHelper final : public cip::tibia::shared::TDynamicResourcesHelper
{
    Q_OBJECT
public:
    using cip::tibia::shared::TDynamicResourcesHelper::TDynamicResourcesHelper;
};

// TOptimized1PixelBorderImageProvider — live QQuickImageProvider in
// imageproviders/optimizedborderimageprovider.h (CipSoft requestImage).

}
}


