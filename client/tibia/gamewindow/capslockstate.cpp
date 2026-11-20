#include "gamewindow/capslockstate.h"

#include "platform/platform.h"

namespace tibia::gamewindow {

bool isCapsLockActive()
{
    return tibia::platform::isCapsLockActive();
}

}
