# Mods/: optional packs with mod.json.
#
#   client/mods/<name>/mod.json
#   client/mods/<name>/CMakeLists.txt   # build-time: qtclient_add_mod / brand.cmake
#   launcher/mods/<name>/…              # launcher-only packs (chrome overlays)
#
# Discovery here is for **build-time** pack targets and brand.cmake only.
# Runtime loading is ModLoader::loadFromDirectory (mods/<id>/ next to the client).
# Linking product C++ into `tibia` is not required — packs are MODULE/SIDE_MODULE.
#
# Variables (QTCLIENT_* — same prefix as runtime QTCLIENT_MODS_DIR):
#   QTCLIENT_ENABLE_MODS — when OFF, skip every mod (stock CIP only). Default ON.
#   QTCLIENT_MODS_DIR    — client mods search path (default: ${TIBIA_ROOT}/client/mods)
#   QTCLIENT_MODS        — semicolon list of mod folder names; empty = all non-template
#
# Underscore-prefixed dirs (_sample) are templates unless listed in QTCLIENT_MODS.
# Stock CIP QML lives in client/upstream/, not under mods/.

option(QTCLIENT_ENABLE_MODS "Discover and link mods/ packs" ON)
set(QTCLIENT_MODS_DIR "${TIBIA_ROOT}/client/mods" CACHE PATH "Directory containing client/mods/<name>/")
set(QTCLIENT_MODS "" CACHE STRING "Semicolon list of mod names to enable (empty = all non-template)")

function(tibia_list_mods out_var)
  set(names "")
  if(EXISTS "${QTCLIENT_MODS_DIR}")
    file(GLOB _mod_dirs LIST_DIRECTORIES true "${QTCLIENT_MODS_DIR}/*")
    foreach(d IN LISTS _mod_dirs)
      if(IS_DIRECTORY "${d}" AND EXISTS "${d}/mod.json")
        get_filename_component(n "${d}" NAME)
        list(APPEND names "${n}")
      endif()
    endforeach()
  endif()
  set(${out_var} "${names}" PARENT_SCOPE)
endfunction()

function(tibia_mod_is_template mod_name out_var)
  string(SUBSTRING "${mod_name}" 0 1 _first)
  if(_first STREQUAL "_")
    set(${out_var} TRUE PARENT_SCOPE)
  else()
    set(${out_var} FALSE PARENT_SCOPE)
  endif()
endfunction()

# Resolve which packs to build (brand + qtclient_add_mod). Sets QTCLIENT_ENABLED_MODS.
function(tibia_resolve_enabled_mods)
  set(enabled "")
  if(NOT QTCLIENT_ENABLE_MODS)
    set(QTCLIENT_ENABLED_MODS "${enabled}" PARENT_SCOPE)
    return()
  endif()
  tibia_list_mods(_all)
  if(QTCLIENT_MODS)
    foreach(n IN LISTS QTCLIENT_MODS)
      if(n IN_LIST _all)
        list(APPEND enabled "${n}")
      else()
        message(WARNING "qtclient mods: '${n}' listed in QTCLIENT_MODS but not found under ${QTCLIENT_MODS_DIR}")
      endif()
    endforeach()
  else()
    foreach(n IN LISTS _all)
      tibia_mod_is_template("${n}" _tmpl)
      if(NOT _tmpl)
        list(APPEND enabled "${n}")
      endif()
    endforeach()
  endif()
  set(QTCLIENT_ENABLED_MODS "${enabled}" PARENT_SCOPE)
endfunction()

function(tibia_add_mod_subdirectory mod_name)
  set(dir "${QTCLIENT_MODS_DIR}/${mod_name}")
  if(NOT EXISTS "${dir}/CMakeLists.txt")
    message(STATUS "qtclient mods: ${mod_name} has mod.json but no CMakeLists.txt — skip link")
    return()
  endif()
  message(STATUS "qtclient mods: enabling ${mod_name}")
  add_subdirectory("${dir}" "${CMAKE_CURRENT_BINARY_DIR}/mods/${mod_name}")
endfunction()

function(tibia_add_enabled_mods)
  tibia_resolve_enabled_mods()
  foreach(_qt_mod IN LISTS QTCLIENT_ENABLED_MODS)
    # Optional brand.cmake overrides stock TIBIA_PRODUCT_* before the pack links.
    set(_brand "${QTCLIENT_MODS_DIR}/${_qt_mod}/brand.cmake")
    if(EXISTS "${_brand}")
      include("${_brand}")
    endif()
    tibia_add_mod_subdirectory("${_qt_mod}")
  endforeach()
  if(QTCLIENT_ENABLE_MODS AND QTCLIENT_ENABLED_MODS STREQUAL "")
    message(STATUS "qtclient mods: enabled, none discovered (stock CIP)")
  endif()
  # add_subdirectory / include ran in this function scope — push brand out to caller.
  set(TIBIA_PRODUCT_NAME "${TIBIA_PRODUCT_NAME}" PARENT_SCOPE)
  set(TIBIA_PRODUCT_ORG "${TIBIA_PRODUCT_ORG}" PARENT_SCOPE)
  set(TIBIA_PRODUCT_URL "${TIBIA_PRODUCT_URL}" PARENT_SCOPE)
  set(TIBIA_DEFAULT_CDN "${TIBIA_DEFAULT_CDN}" PARENT_SCOPE)
  set(TIBIA_CLIENT_PACKAGE_DIR "${TIBIA_CLIENT_PACKAGE_DIR}" PARENT_SCOPE)
  if(DEFINED TIBIA_PRODUCT_BUNDLE_ID)
    set(TIBIA_PRODUCT_BUNDLE_ID "${TIBIA_PRODUCT_BUNDLE_ID}" PARENT_SCOPE)
  endif()
  set(QTCLIENT_ENABLED_MODS "${QTCLIENT_ENABLED_MODS}" PARENT_SCOPE)
endfunction()
