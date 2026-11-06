# Default OTC wire pack — absorbed into tibia via qtclient_add_mod.
# Keep ABSORB_INTO_TIP until this pack peels onto runtime MODULE + host ABI.
# Without it, WASM tips get MAIN_MODULE=2 and Safari dies in generateFuncType
# (sig.slice on undefined) during early boot.
include("${TIBIA_ROOT}/cmake/QtClientMod.cmake")

qtclient_add_mod(otclient
  ABSORB_INTO_TIP
  SOURCES src/otclient_mod.cpp
  MOD_JSON "${CMAKE_CURRENT_SOURCE_DIR}/mod.json"
)

message(STATUS "qtclient mods: otclient default pack (qtclient_add_mod)")
