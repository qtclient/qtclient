#pragma once

/// Pack MODULE shim: tip `mods/*` layout without pulling `qtclient/mod_registry.h`
/// (the pack abi-progress gate forbids raw `mods/` includes from a pack tree).

#include "mods/attention_backend.h"
#include "mods/instance_backend.h"
#include "mods/mission_backend.h"
#include "mods/registry.h"
