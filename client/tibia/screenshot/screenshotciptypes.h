#pragma once

// CIP-named screenshot surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::screenshot {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TOutstandingScreenshot final : public cip::tibia::screenshot::TOutstandingScreenshot
{
    Q_OBJECT
public:
    using cip::tibia::screenshot::TOutstandingScreenshot::TOutstandingScreenshot;
};

class TScreenshotManager final : public cip::tibia::screenshot::TScreenshotManager
{
    Q_OBJECT
public:
    using cip::tibia::screenshot::TScreenshotManager::TScreenshotManager;
};

} // namespace tibia::screenshot

