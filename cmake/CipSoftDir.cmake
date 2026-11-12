# C5: optional kind-A root from make dump (PLAN §6).
# Layout: ${QTCLIENT_CIPSOFT_DIR}/client/{qml,shaders,strings,conf,proto}/…
# When unset or missing, fall back to vendored client/upstream/ (today's tree).

if(DEFINED ENV{QTCLIENT_CIPSOFT_DIR} AND NOT "$ENV{QTCLIENT_CIPSOFT_DIR}" STREQUAL "")
  set(_cipsoft_dir "$ENV{QTCLIENT_CIPSOFT_DIR}")
elseif(DEFINED QTCLIENT_CIPSOFT_DIR AND NOT "${QTCLIENT_CIPSOFT_DIR}" STREQUAL "")
  set(_cipsoft_dir "${QTCLIENT_CIPSOFT_DIR}")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/../cipsoft-data/client/qml")
  # Umbrella mount: apps/cipsoft-data next to apps/qtclient
  set(_cipsoft_dir "${CMAKE_SOURCE_DIR}/../cipsoft-data")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/cipsoft/client/qml")
  set(_cipsoft_dir "${CMAKE_SOURCE_DIR}/cipsoft")
else()
  set(_cipsoft_dir "")
endif()

if(EXISTS "${_cipsoft_dir}/client/qml")
  set(CIPSOFT_KIND_A_DIR "${_cipsoft_dir}" CACHE PATH "CipSoft kind-A dump root" FORCE)
  set(CIPSOFT_QML_DIR "${_cipsoft_dir}/client/qml" CACHE PATH "" FORCE)
  set(CIPSOFT_SHADERS_DIR "${_cipsoft_dir}/client/shaders" CACHE PATH "" FORCE)
  set(CIPSOFT_STRINGS_DIR "${_cipsoft_dir}/client/strings" CACHE PATH "" FORCE)
  set(CIPSOFT_CONF_DIR "${_cipsoft_dir}/client/conf" CACHE PATH "" FORCE)
  set(CIPSOFT_PROTO_DIR "${_cipsoft_dir}/client/proto" CACHE PATH "" FORCE)
  message(STATUS "CipSoft kind A: ${CIPSOFT_KIND_A_DIR}")
else()
  set(CIPSOFT_KIND_A_DIR "" CACHE PATH "CipSoft kind-A dump root (empty = use client/upstream)" FORCE)
  set(CIPSOFT_QML_DIR "${CMAKE_SOURCE_DIR}/client/upstream/qml" CACHE PATH "" FORCE)
  set(CIPSOFT_SHADERS_DIR "${CMAKE_SOURCE_DIR}/client/upstream/shaders" CACHE PATH "" FORCE)
  set(CIPSOFT_STRINGS_DIR "${CMAKE_SOURCE_DIR}/client/upstream/strings" CACHE PATH "" FORCE)
  set(CIPSOFT_CONF_DIR "${CMAKE_SOURCE_DIR}/client/upstream/conf" CACHE PATH "" FORCE)
  set(CIPSOFT_PROTO_DIR "${CMAKE_SOURCE_DIR}/client/upstream/proto" CACHE PATH "" FORCE)
endif()
