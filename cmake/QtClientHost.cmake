# QtClient tip host — config for packs built with or against this tip.
#
# Packs see only include/qtclient/abi (qtclient_add_mod): desktop packs are runtime
# MODULEs (mods/<id>/native/mod.*, loaded by ModLoader); wasm packs are STATIC_ABI
# archives linked into tibia.wasm (the wasm Qt kit is static). QtClient::Host is the
# abi include root; nothing links against tibia's own symbols.

include_guard(GLOBAL)

function(qtclient_host_finalize)
  if(NOT TARGET tibia)
    message(FATAL_ERROR "qtclient_host_finalize: tibia target missing")
  endif()

  if(NOT TARGET qtclient_host)
    add_library(qtclient_host INTERFACE)
    target_include_directories(qtclient_host INTERFACE
      "$<BUILD_INTERFACE:${TIBIA_ROOT}/include>"
      "$<INSTALL_INTERFACE:include>")
  endif()

  if(NOT TARGET QtClient::Host)
    add_library(QtClient::Host ALIAS qtclient_host)
  endif()

  set(_qtclient_config_dir "${CMAKE_BINARY_DIR}/cmake")
  file(MAKE_DIRECTORY "${_qtclient_config_dir}")
  file(WRITE "${_qtclient_config_dir}/QtClientConfig.cmake" "# Generated — build tree (abi include root; packs never link tibia)
if(NOT TARGET QtClient::Host)
  add_library(QtClient::Host INTERFACE IMPORTED)
  set_target_properties(QtClient::Host PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES \"${TIBIA_ROOT}/include\")
endif()
set(QtClient_FOUND TRUE)
set(QTCLIENT_ROOT \"${TIBIA_ROOT}\")
")
  message(STATUS "qtclient_host: packs via abi/ (desktop MODULE, wasm STATIC_ABI; no ENABLE_EXPORTS)")
endfunction()
