#pragma once

namespace tibia {
namespace gamewindow {

/// Whether Caps Lock is on, read from the OS key state (false where the
/// platform gives no such state).
bool isCapsLockActive();

}
}
