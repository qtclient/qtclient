# Sample pack — unified mods/<id>/ layout via qtclient_add_mod.
# Enable with -DQTCLIENT_MODS=_sample or copy to client/mods/sample.
include("${TIBIA_ROOT}/cmake/QtClientMod.cmake")

qtclient_add_mod(sample
  SOURCES src/sample_mod.cpp
  QML_DIR "${CMAKE_CURRENT_SOURCE_DIR}/qml"
  QML_MODULE sample
  MOD_JSON "${CMAKE_CURRENT_SOURCE_DIR}/mod.json"
)

message(STATUS "qtclient mods: _sample SDK pack (qtclient_add_mod)")
