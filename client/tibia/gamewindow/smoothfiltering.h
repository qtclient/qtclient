#pragma once

namespace tibia {
namespace gamewindow {

/// CIP clientSettingSmoothFiltering getters: the client option antialiasingMode is 1 (smooth).
/// Read through config::TClientOptions::instance() (configinput is typing the options).
bool smoothFilteringEnabled();

}
}
