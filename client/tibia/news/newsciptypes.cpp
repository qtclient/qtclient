#include "news/newsciptypes.h"

namespace tibia::news {

namespace {
struct CipTypedKeep {
    TNewsDiskIOAsync tNewsDiskIOAsync;
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

} // namespace tibia::news
