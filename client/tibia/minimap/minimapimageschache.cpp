#include "minimap/minimapimageschache.h"

#include "clockservice.h"

#include <algorithm>
#include <limits>

namespace tibia::minimap {

namespace {

/// Past this many images one is pushed out before the next is stored.
constexpr size_t MaximumCachedImages = 200;

}

TMinimapImagesChache::TMinimapImagesChache()
    : cip::tibia::minimap::TMinimapImagesChache(nullptr)
    , m_clock(std::make_shared<shared::TClockService>(0))
{
}

TMinimapImagesChache::~TMinimapImagesChache() = default;

void TMinimapImagesChache::onImageLoaded(const QString& Filepath, const QImage& Image)
{
    if (m_images.size() > MaximumCachedImages) {
        qint64 oldest = std::numeric_limits<qint64>::max();
        for (const auto& [path, cached] : m_images)
            oldest = std::min(oldest, cached.storedAtMillis);
        for (auto it = m_images.begin(); it != m_images.end(); ++it) {
            if (it->second.storedAtMillis == oldest) {
                m_images.erase(it);
                break;
            }
        }
    }
    const qint64 now = m_clock->nowMillis();
    TCachedImage& cached = m_images[Filepath];
    cached.storedAtMillis = now;
    cached.image = Image;
}

}
