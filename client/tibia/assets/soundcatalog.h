#pragma once

#include "assets/assetcatalog.h"

namespace tibia {
namespace assets {

/// The catalog of the sound assets (CIP: TSoundCatalog): it reads nothing
/// beyond the asset entries themselves.
class TSoundCatalog final : public TAssetCatalog
{
public:
    TSoundCatalog() = default;

protected:
    void assetRead(const TAsset& asset, const QJsonObject& json) override;
};

}
}
