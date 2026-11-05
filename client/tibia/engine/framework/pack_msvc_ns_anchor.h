#pragma once

// Force-included first on MSVC linked-pack TUs (see QtClientMod.cmake).
//
// Pack TUs lack engine /FI pch. MSVC (with /permissive-) can lose sibling
// `tibia::container` after the first `namespace tibia { namespace appearances … }`
// in the session/appearance include graph (C3083). Pre-opening the tibia
// nest emptily makes later classic reopenings merge onto a stable root.

namespace tibia {
namespace appearances {}
namespace creatures {}
namespace container {}
namespace client {}
namespace game {}
namespace gamewindow {}
namespace qmlcomponents {}
namespace worldmap {}
namespace mods {}
namespace idleot {}
namespace protobuf {
namespace protocol {}
}
}
