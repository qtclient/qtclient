# Mods: packs with mod.json next to the tip. The host never builds a pack.
#
# Packs build on their own against the pack SDK (find_package(QtClientSdk), see
# sdk/CMakeLists.txt + cmake/QtClientMod.cmake) and install a tree:
#   <prefix>/mods/<id>/{mod.json, native/mod.* (desktop MODULE), qml/, overrides/, resources/, …}
#   <prefix>/lib/cmake/QtClientPack_<c_id>/   (wasm STATIC_ABI: libqtclient_mod_<id>.a export)
#   <prefix>/share/qtclient-pack/<id>/brand.cmake (optional TIBIA_PRODUCT_* identity)
#
# The host takes installed packs as inputs:
#   -DQTCLIENT_PACKS=<prefix>[;<prefix>…]
#     desktop: stage mods/<id> (incl. native/mod.*) next to the tip; nothing is linked.
#     wasm:    link QtClientPack::<c_id> (a static-ABI archive built with the same SDK)
#              into tibia, generate static_abi_mods.cpp for ModLoader, stage mods/<id>
#              (no native/). Qt-free packs with native/mod.wasm stay SIDE_MODULEs
#              (tip MAIN_MODULE=2).
#   -DQTCLIENT_DEV_PACK_SOURCES=<pack>/client[;…]  desktop DEV only: disk-QML roots
#     (qml/ + overrides/) for the TIBIA_QML_DISK hot-reload loop. Not a build input.
#
# In-tree client/mods/<name>/ packs are FS-only (mod.json + qml/overrides); a
# client/mods/<name>/CMakeLists.txt is an error (build it against the SDK).
# No tip export allowlist.

option(QTCLIENT_ENABLE_MODS "Stage mods/ packs with tibia" ON)
set(QTCLIENT_MODS_DIR "${TIBIA_ROOT}/client/mods" CACHE PATH "Directory containing client/mods/<name>/")
set(QTCLIENT_MODS "" CACHE STRING "Semicolon list of mod names to enable (empty = all non-template)")
set(QTCLIENT_PACKS "" CACHE STRING "Semicolon list of installed pack prefixes (built against QtClientSdk)")
set(QTCLIENT_DEV_PACK_SOURCES "" CACHE STRING "Desktop DEV: pack client/ source dirs for disk QML (no build)")
foreach(_gone QTCLIENT_LINKED_PACK QTCLIENT_LINKED_PACKS QTCLIENT_LINKED_PACK_LIBS)
  if(${_gone})
    message(FATAL_ERROR "${_gone} was removed: the host no longer builds packs. Build the pack against "
      "the SDK (find_package(QtClientSdk)), install it, and pass -DQTCLIENT_PACKS=<install prefix> "
      "(apps/qtclient/docs/mods.md, Build).")
  endif()
endforeach()

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

# Desktop DEV disk-QML source roots (client/tibia/CMakeLists.txt TIBIA_QML_DISK).
function(tibia_linked_pack_dirs out_var)
  set(_dirs "")
  foreach(_p IN LISTS QTCLIENT_DEV_PACK_SOURCES)
    if(_p)
      list(APPEND _dirs "${_p}")
    endif()
  endforeach()
  list(REMOVE_DUPLICATES _dirs)
  set(${out_var} "${_dirs}" PARENT_SCOPE)
endfunction()

# One installed pack prefix → stage entries, optional brand, optional static-ABI link.
function(_tibia_add_installed_pack prefix)
  get_filename_component(_prefix "${prefix}" ABSOLUTE)
  file(GLOB _mod_jsons "${_prefix}/mods/*/mod.json")
  if(NOT _mod_jsons)
    message(FATAL_ERROR "QTCLIENT_PACKS=${prefix}: no mods/<id>/mod.json (install the pack: cmake --install)")
  endif()
  foreach(_json IN LISTS _mod_jsons)
    get_filename_component(_mod_dir "${_json}" DIRECTORY)
    get_filename_component(_id "${_mod_dir}" NAME)
    string(MAKE_C_IDENTIFIER "${_id}" _c_id)

    set(_brand "${_prefix}/share/qtclient-pack/${_id}/brand.cmake")
    if(EXISTS "${_brand}")
      include("${_brand}")
    endif()

    file(GLOB _native "${_mod_dir}/native/mod.*")
    set(_cfg_dir "${_prefix}/lib/cmake/QtClientPack_${_c_id}")
    # A static-ABI install (QtClientPack_<c_id> export) is linked into tibia: the wasm
    # mode, also any static-Qt tip. A MODULE install (native/mod.*) is staged and
    # loaded at run time; on wasm only a Qt-free pack ships one (SIDE_MODULE).
    if(EXISTS "${_cfg_dir}/QtClientPack_${_c_id}Config.cmake")
      # NO_CMAKE_FIND_ROOT_PATH: the wasm toolchain re-roots searches into the sysroot.
      find_package(QtClientPack_${_c_id} CONFIG REQUIRED PATHS "${_prefix}" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
      set(_tgt "QtClientPack::${_c_id}")
      if(NOT TARGET ${_tgt})
        message(FATAL_ERROR "QTCLIENT_PACKS: ${_cfg_dir} did not define ${_tgt}")
      endif()
      target_link_libraries(tibia PRIVATE ${_tgt})
      set_property(GLOBAL APPEND PROPERTY QTCLIENT_STATIC_ABI_MOD_IDS "${_id}")
      message(STATUS "qtclient mods: ${_id} static-ABI ← ${_cfg_dir}")
    elseif(_native AND (EMSCRIPTEN OR WASM))
      set_property(GLOBAL APPEND PROPERTY QTCLIENT_SIDE_MODULE_MOD_IDS "${_id}")
      message(STATUS "qtclient mods: ${_id} wasm SIDE_MODULE (Qt-free) ← ${_native}")
    elseif(_native)
      set_property(GLOBAL APPEND PROPERTY QTCLIENT_RUNTIME_MOD_IDS "${_id}")
      message(STATUS "qtclient mods: ${_id} runtime MODULE ← ${_native}")
    else()
      message(STATUS "qtclient mods: ${_id} FS-only pack ← ${_mod_dir}")
    endif()
    set_property(GLOBAL APPEND PROPERTY QTCLIENT_MOD_TIP_STAGE "${_id}|${_mod_dir}")
  endforeach()
  foreach(_v TIBIA_PRODUCT_NAME TIBIA_PRODUCT_ORG TIBIA_PRODUCT_URL TIBIA_DEFAULT_CDN
             TIBIA_PRODUCT_BUNDLE_ID TIBIA_PRODUCT_ICNS TIBIA_PRODUCT_ANDROID_APP_NAME)
    if(DEFINED ${_v})
      set(${_v} "${${_v}}" PARENT_SCOPE)
    endif()
  endforeach()
endfunction()

function(tibia_add_enabled_mods)
  tibia_resolve_enabled_mods()
  foreach(_qt_mod IN LISTS QTCLIENT_ENABLED_MODS)
    if(EXISTS "${QTCLIENT_MODS_DIR}/${_qt_mod}/CMakeLists.txt")
      message(FATAL_ERROR "qtclient mods: ${QTCLIENT_MODS_DIR}/${_qt_mod} has a CMakeLists.txt — the host does not "
        "build packs. Build it against QtClientSdk and pass its install prefix in QTCLIENT_PACKS.")
    endif()
    set(_brand "${QTCLIENT_MODS_DIR}/${_qt_mod}/brand.cmake")
    if(EXISTS "${_brand}")
      include("${_brand}")
    endif()
  endforeach()

  if(QTCLIENT_ENABLE_MODS)
    foreach(_prefix IN LISTS QTCLIENT_PACKS)
      if(_prefix)
        _tibia_add_installed_pack("${_prefix}")
      endif()
    endforeach()
  endif()

  if(QTCLIENT_ENABLE_MODS AND QTCLIENT_ENABLED_MODS STREQUAL "" AND NOT QTCLIENT_PACKS)
    message(STATUS "qtclient mods: enabled, none discovered (stock CIP)")
  endif()
  foreach(_v TIBIA_PRODUCT_NAME TIBIA_PRODUCT_ORG TIBIA_PRODUCT_URL TIBIA_DEFAULT_CDN
             TIBIA_PRODUCT_BUNDLE_ID TIBIA_PRODUCT_ICNS TIBIA_PRODUCT_ANDROID_APP_NAME)
    if(DEFINED ${_v})
      set(${_v} "${${_v}}" PARENT_SCOPE)
    endif()
  endforeach()
  set(QTCLIENT_ENABLED_MODS "${QTCLIENT_ENABLED_MODS}" PARENT_SCOPE)
endfunction()

# Generate the static-ABI pack table (wasm) + stage installed packs next to the tip.
# Must run from the directory that created target `tibia` (after tibia_add_enabled_mods).
function(tibia_apply_linked_pack_staging)
  if(NOT TARGET tibia)
    message(FATAL_ERROR "tibia_apply_linked_pack_staging: target tibia missing")
  endif()

  get_property(_static_ids GLOBAL PROPERTY QTCLIENT_STATIC_ABI_MOD_IDS)
  if(_static_ids)
    list(REMOVE_DUPLICATES _static_ids)
    set(_gen_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
    file(MAKE_DIRECTORY "${_gen_dir}")
    set(_gen "${_gen_dir}/static_abi_mods.cpp")
    set(STATIC_ABI_MOD_IDS "${_static_ids}")
    set(OUTPUT "${_gen}")
    include("${TIBIA_ROOT}/cmake/GenerateStaticAbiMods.cmake")
    target_sources(tibia PRIVATE "${_gen}")
    target_compile_definitions(tibia PRIVATE QTCLIENT_HAS_STATIC_ABI_MODS=1)
    message(STATUS "qtclient mods: static-ABI pack table ← ${_static_ids}")
  endif()

  get_property(_runtime_ids GLOBAL PROPERTY QTCLIENT_RUNTIME_MOD_IDS)
  get_property(_side_ids GLOBAL PROPERTY QTCLIENT_SIDE_MODULE_MOD_IDS)
  if(_side_ids AND (EMSCRIPTEN OR WASM))
    # Only Qt-free (NO_QT) packs are wasm SIDE_MODULEs; the tip must then be
    # MAIN_MODULE. Packs import zero tip symbols (host-pushed API); no tip
    # export allowlist. Qt packs are STATIC_ABI and keep the tip a plain executable.
    target_link_options(tibia PRIVATE
      "SHELL:-sMAIN_MODULE=2"
      "SHELL:-sERROR_ON_UNDEFINED_SYMBOLS=0")
    target_compile_definitions(tibia PRIVATE QTCLIENT_HAS_RUNTIME_MODS=1)
    message(STATUS "qtclient mods: WASM MAIN_MODULE=2 for Qt-free SIDE_MODULE packs ← ${_side_ids}")
  elseif(_runtime_ids)
    target_compile_definitions(tibia PRIVATE QTCLIENT_HAS_RUNTIME_MODS=1)
    message(STATUS "qtclient mods: runtime MODULE packs ← ${_runtime_ids}")
  endif()

  # Brand from an installed pack's brand.cmake (TIBIA_PRODUCT_* came back via PARENT_SCOPE).
  if(TIBIA_PRODUCT_ICNS AND APPLE AND NOT EMSCRIPTEN AND EXISTS "${TIBIA_PRODUCT_ICNS}")
    get_filename_component(_icns_name "${TIBIA_PRODUCT_ICNS}" NAME_WE)
    set_source_files_properties("${TIBIA_PRODUCT_ICNS}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(tibia PRIVATE "${TIBIA_PRODUCT_ICNS}")
    set_target_properties(tibia PROPERTIES MACOSX_BUNDLE_ICON_FILE "${_icns_name}")
  endif()
  if(ANDROID AND TIBIA_PRODUCT_ANDROID_APP_NAME)
    set_target_properties(tibia PROPERTIES QT_ANDROID_APP_NAME "${TIBIA_PRODUCT_ANDROID_APP_NAME}")
  endif()

  get_property(_stages GLOBAL PROPERTY QTCLIENT_MOD_TIP_STAGE)
  foreach(_entry IN LISTS _stages)
    string(REPLACE "|" ";" _parts "${_entry}")
    list(GET _parts 0 _mod_id)
    list(GET _parts 1 _mod_dir)
    # Installed packs are inputs: copy as-is (wasm: never native/; the C++ is in tibia.wasm).
    set(_drop_native "")
    if(EMSCRIPTEN OR WASM)
      get_property(_side GLOBAL PROPERTY QTCLIENT_SIDE_MODULE_MOD_IDS)
      if(NOT _mod_id IN_LIST _side)
        set(_drop_native COMMAND ${CMAKE_COMMAND} -E rm -rf "$<TARGET_FILE_DIR:tibia>/mods/${_mod_id}/native")
      endif()
    endif()
    add_custom_command(TARGET tibia POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E rm -rf "$<TARGET_FILE_DIR:tibia>/mods/${_mod_id}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:tibia>/mods/${_mod_id}"
      COMMAND ${CMAKE_COMMAND} -E copy_directory "${_mod_dir}" "$<TARGET_FILE_DIR:tibia>/mods/${_mod_id}"
      ${_drop_native}
      COMMAND ${CMAKE_COMMAND}
        -DNATIVE_DIR="$<TARGET_FILE_DIR:tibia>/mods/${_mod_id}/native"
        -P "${TIBIA_ROOT}/cmake/ScrubEmptyNative.cmake"
      COMMENT "stage installed pack ${_mod_id} next to tip")
  endforeach()
endfunction()
