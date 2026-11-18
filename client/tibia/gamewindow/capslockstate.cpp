#include "capslockstate.h"

#if defined(__APPLE__)
#include <Carbon/Carbon.h>
#endif

namespace tibia::gamewindow {

bool isCapsLockActive()
{
#if defined(__APPLE__) && !defined(__EMSCRIPTEN__)
    return (GetCurrentKeyModifiers() >> alphaLockBit) & 1;
#else
    return false;
#endif
}

}
