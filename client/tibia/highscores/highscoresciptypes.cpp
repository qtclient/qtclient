#include "highscores/highscoresciptypes.h"

namespace tibia::highscores {

namespace {
} // namespace

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)parent;
}

} // namespace tibia::highscores
