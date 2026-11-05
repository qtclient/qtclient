# Default OTC wire pack — absorbed into tibia via qtclient_add_mod.
include("${TIBIA_ROOT}/cmake/QtClientMod.cmake")

qtclient_add_mod(otclient
  SOURCES src/otclient_mod.cpp
  MOD_JSON "${CMAKE_CURRENT_SOURCE_DIR}/mod.json"
)

message(STATUS "qtclient mods: otclient default pack (qtclient_add_mod)")
