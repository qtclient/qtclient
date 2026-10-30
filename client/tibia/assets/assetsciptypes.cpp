#include "assets/assetsciptypes.h"

namespace tibia::assets {

namespace {
struct CipTypedKeep {
    TAsset tAsset;
    TAssetCatalog tAssetCatalog;
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

} // namespace tibia::assets
