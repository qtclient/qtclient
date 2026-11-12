#include "gameobject.h"

int GameObject::getUseCount()
{
    try {
        const auto self = shared_from_this();
        return static_cast<int>(self.use_count()) - 1;
    } catch (...) {
        return 0;
    }
}
