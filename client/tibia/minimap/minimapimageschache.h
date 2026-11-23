#pragma once

#include <cip_gen.h>

#include <QImage>
#include <QString>

#include <map>
#include <memory>

namespace tibia::shared {
class TClockService;
}

namespace tibia {
namespace minimap {

/// Images the minimap draws, by file path (CIP: TMinimapImagesChache, sic):
/// at most about 200; past that, a newly loaded image first pushes out one of
/// those stored longest ago.
class TMinimapImagesChache final : public cip::tibia::minimap::TMinimapImagesChache
{
    Q_OBJECT
public:
    TMinimapImagesChache();
    ~TMinimapImagesChache() override;

public Q_SLOTS:
    /// Stores the image read from `Filepath`, with the time it arrived.
    void onImageLoaded(const QString& Filepath, const QImage& Image);

private:
    struct TCachedImage
    {
        qint64 storedAtMillis = 0;
        QImage image;
    };

    std::map<QString, TCachedImage> m_images;
    std::shared_ptr<shared::TClockService> m_clock;
};

}
}
