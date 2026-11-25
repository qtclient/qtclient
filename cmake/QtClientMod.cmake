# QtClientMod — build a qtclient pack's C++ against the pack SDK (QtClientSdk) only.
#
# Shipped in the SDK (lib/cmake/QtClientSdk/) and loaded by find_package(QtClientSdk).
# Never included by the host build: the host consumes a pack's *installed* tree
# (cmake/TibiaMods.cmake, -DQTCLIENT_PACKS=<prefix>).
#
#   qtclient_add_mod(<id>
#     SOURCES …
#     [QML_DIR dir]
#     [OVERRIDES_DIR dir]
#     [RESOURCES_DIR dir]
#     [MOD_JSON path]
#     [QML_MODULE name]
#     [GUI_PRIVATE]            # link Qt6::GuiPrivate + Qt6::QuickPrivate (RHI scene-graph nodes)
#     [NO_QT]                  # pure C ABI pack (no Qt link); may stay a wasm SIDE_MODULE
#     [STATIC_ABI | MODULE]    # force the mode (default: the SDK's QTCLIENT_SDK_PACK_MODE)
#   )
#   qtclient_mod_install(<id>
#     [EXTRA_TARGETS tgt…]     # STATIC_ABI: qt_add_resources / qt_add_shaders OUTPUT_TARGETS
#     [BRAND_FILES file…])     # installed to share/qtclient-pack/<id>/ (brand.cmake first)
#
# Two modes, one source tree (host API v9, abi/mod_register.h):
#
#   MODULE (desktop): add_library(qtclient_mod_<id> MODULE) →
#     mods/<id>/native/mod.{dylib,so,dll}, sole export `qtclient_mod_register`.
#     Links Qt from the same (shared) kit as the tip; AUTOMOC/AUTORCC; RPATH to the
#     bundled Qt. The tip's ModLoader loads it with QLibrary.
#
#   STATIC_ABI (wasm; the CIP wasm Qt kit is static, so a Qt pack cannot be a
#     SIDE_MODULE — a second static Qt would be a second QCoreApplication / QML
#     registry): add_library(qtclient_mod_<id> STATIC) with QTCLIENT_MOD_STATIC_ABI.
#     The entry becomes `qtclient_mod_<c_id>_register_abi`. qtclient_mod_install
#     exports it as QtClientPack::<c_id> (lib/cmake/QtClientPack_<c_id>/); the host
#     build takes that installed archive as an input and generates the table
#     ModLoader walks (cmake/GenerateStaticAbiMods.cmake). No native/.
#
# Either way the pack compiles with only its own dirs + QtClientSdk::Abi
# (include/qtclient/abi): no host include dir, host PCH, host definition or host link.
#
# Install layout (<prefix> = the pack's CMAKE_INSTALL_PREFIX):
#   <prefix>/mods/<id>/{mod.json, native/mod.* (MODULE), qml/<QML_MODULE>/, overrides/, resources/, …}
#   <prefix>/lib/libqtclient_mod_<id>.a + lib/cmake/QtClientPack_<c_id>/   (STATIC_ABI)
#   <prefix>/share/qtclient-pack/<id>/brand.cmake                         (optional)

include_guard(GLOBAL)

set(_QTCLIENT_MOD_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}")

if(NOT TARGET QtClientSdk::Abi)
  message(FATAL_ERROR "QtClientMod.cmake: load it with find_package(QtClientSdk) "
    "(the pack SDK: apps/qtclient/sdk, scripts/lifecycle/build/sdk.sh)")
endif()

# True when the Qt kit in use is static (the CIP wasm kit).
function(_qtclient_mod_qt_is_static out_var)
  set(_static FALSE)
  if(TARGET Qt6::Core)
    get_target_property(_type Qt6::Core TYPE)
    if(_type STREQUAL "STATIC_LIBRARY")
      set(_static TRUE)
    endif()
  endif()
  set(${out_var} ${_static} PARENT_SCOPE)
endfunction()

function(qtclient_add_mod MOD_ID)
  cmake_parse_arguments(ARG "GUI_PRIVATE;NO_QT;STATIC_ABI;MODULE"
    "QML_DIR;OVERRIDES_DIR;RESOURCES_DIR;MOD_JSON;QML_MODULE" "SOURCES" ${ARGN})
  if("ABSORB_INTO_QTCLIENT" IN_LIST ARG_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "qtclient_add_mod(${MOD_ID}): ABSORB_INTO_QTCLIENT was removed (host API v9) — "
      "packs build against QtClientSdk as MODULE (desktop) / STATIC_ABI (wasm)")
  endif()
  if(ARG_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "qtclient_add_mod(${MOD_ID}): unknown arguments: ${ARG_UNPARSED_ARGUMENTS}")
  endif()
  if(NOT ARG_SOURCES)
    message(FATAL_ERROR "qtclient_add_mod(${MOD_ID}): SOURCES required")
  endif()
  if(ARG_STATIC_ABI AND ARG_MODULE)
    message(FATAL_ERROR "qtclient_add_mod(${MOD_ID}): STATIC_ABI and MODULE are exclusive")
  endif()
  if(TARGET tibia)
    message(FATAL_ERROR "qtclient_add_mod(${MOD_ID}): packs build apart from the host "
      "(find_package(QtClientSdk)); the host takes the installed pack via -DQTCLIENT_PACKS")
  endif()
  if(NOT ARG_QML_MODULE)
    set(ARG_QML_MODULE "${MOD_ID}")
  endif()
  string(MAKE_C_IDENTIFIER "${MOD_ID}" _c_id)

  set(_outdir "${CMAKE_BINARY_DIR}/mods/${MOD_ID}")
  file(MAKE_DIRECTORY "${_outdir}")

  set(_abs_sources "")
  foreach(_src IN LISTS ARG_SOURCES)
    if(IS_ABSOLUTE "${_src}")
      list(APPEND _abs_sources "${_src}")
    else()
      list(APPEND _abs_sources "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
    endif()
  endforeach()

  # --- mod.json + FS chrome -------------------------------------------------
  set(_mod_json "${ARG_MOD_JSON}")
  if(NOT _mod_json)
    set(_mod_json "${CMAKE_CURRENT_SOURCE_DIR}/mod.json")
  endif()
  if(EXISTS "${_mod_json}")
    configure_file("${_mod_json}" "${_outdir}/mod.json" COPYONLY)
  else()
    file(WRITE "${_outdir}/mod.json" "{
  \"id\": \"${MOD_ID}\",
  \"QTCLIENT_MOD_API\": 1,
  \"autoload\": true,
  \"priority\": 50,
  \"qmlModule\": \"${MOD_ID}\"
}
")
  endif()

  # hostBuildId: ModLoader loads a pack only into the host it was built for (a MODULE
  # shares the tip's Qt and its private API). The SDK records the host's id.
  if(QTCLIENT_SDK_HOST_BUILD_ID AND NOT QTCLIENT_SDK_HOST_BUILD_ID STREQUAL "unknown")
    file(READ "${_outdir}/mod.json" _mod_json_text)
    string(JSON _mod_json_text SET "${_mod_json_text}" hostBuildId "\"${QTCLIENT_SDK_HOST_BUILD_ID}\"")
    file(WRITE "${_outdir}/mod.json" "${_mod_json_text}\n")
  endif()

  if(ARG_QML_DIR AND EXISTS "${ARG_QML_DIR}")
    set(_qml_src_module "${ARG_QML_DIR}/${ARG_QML_MODULE}")
    if(EXISTS "${_qml_src_module}/qmldir" OR EXISTS "${_qml_src_module}")
      set(_qml_stage_from "${ARG_QML_DIR}")
    else()
      set(_qml_stage_from "${CMAKE_CURRENT_BINARY_DIR}/qml-stage-${MOD_ID}")
      file(MAKE_DIRECTORY "${_qml_stage_from}/${ARG_QML_MODULE}")
      file(GLOB _flat_qml LIST_DIRECTORIES false "${ARG_QML_DIR}/*")
      foreach(_entry IN LISTS _flat_qml)
        get_filename_component(_name "${_entry}" NAME)
        if(_name STREQUAL "CMakeLists.txt")
          continue()
        endif()
        file(COPY "${_entry}" DESTINATION "${_qml_stage_from}/${ARG_QML_MODULE}")
      endforeach()
    endif()
    file(REMOVE_RECURSE "${_outdir}/qml")
    file(COPY "${_qml_stage_from}/" DESTINATION "${_outdir}/qml"
      PATTERN "CMakeLists.txt" EXCLUDE)
    execute_process(
      COMMAND ${CMAKE_COMMAND}
        -DMODULE_DIR=${_outdir}/qml/${ARG_QML_MODULE}
        -DMODULE_NAME=${ARG_QML_MODULE}
        -P "${_QTCLIENT_MOD_CMAKE_DIR}/GeneratePackQmldir.cmake"
      RESULT_VARIABLE _qmldir_rc)
    if(NOT _qmldir_rc EQUAL 0)
      message(WARNING "qtclient_add_mod(${MOD_ID}): GeneratePackQmldir failed (${_qmldir_rc})")
    endif()
  endif()
  if(ARG_OVERRIDES_DIR AND EXISTS "${ARG_OVERRIDES_DIR}")
    file(COPY "${ARG_OVERRIDES_DIR}/" DESTINATION "${_outdir}/overrides")
  endif()
  if(ARG_RESOURCES_DIR AND EXISTS "${ARG_RESOURCES_DIR}")
    file(COPY "${ARG_RESOURCES_DIR}/" DESTINATION "${_outdir}/resources")
  endif()

  # --- Qt (the SDK's kit) ----------------------------------------------------
  if(NOT ARG_NO_QT)
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick)
    if(ARG_GUI_PRIVATE AND NOT TARGET Qt6::GuiPrivate)
      find_package(Qt6 REQUIRED COMPONENTS GuiPrivate QuickPrivate)
    endif()
    if(QTCLIENT_SDK_QT_VERSION AND NOT Qt6_VERSION VERSION_EQUAL QTCLIENT_SDK_QT_VERSION)
      message(FATAL_ERROR "qtclient_add_mod(${MOD_ID}): Qt ${Qt6_VERSION} found, but the SDK's host "
        "was built with Qt ${QTCLIENT_SDK_QT_VERSION} (${QTCLIENT_SDK_QT_PREFIX}) — use that kit")
    endif()
  endif()

  # --- mode ----------------------------------------------------------------------
  set(_mode "${QTCLIENT_SDK_PACK_MODE}")
  if(NOT _mode)
    set(_mode MODULE)
  endif()
  if(ARG_STATIC_ABI)
    set(_mode STATIC_ABI)
  elseif(ARG_MODULE)
    set(_mode MODULE)
  elseif(ARG_NO_QT AND (EMSCRIPTEN OR WASM))
    # A Qt-free pack can stay a wasm SIDE_MODULE (tip then MAIN_MODULE=2).
    set(_mode MODULE)
  endif()
  if(_mode STREQUAL "MODULE" AND NOT ARG_NO_QT)
    _qtclient_mod_qt_is_static(_qt_static)
    if(_qt_static)
      message(FATAL_ERROR "qtclient_add_mod(${MOD_ID}): MODULE needs a shared Qt kit "
        "(this kit is static — STATIC_ABI is the wasm mode)")
    endif()
  endif()

  set(_mod_target "qtclient_mod_${MOD_ID}")
  if(_mode STREQUAL "STATIC_ABI")
    file(REMOVE_RECURSE "${_outdir}/native")
    add_library(${_mod_target} STATIC ${_abs_sources})
    set_target_properties(${_mod_target} PROPERTIES EXPORT_NAME "${_c_id}")
  else()
    file(MAKE_DIRECTORY "${_outdir}/native")
    add_library(${_mod_target} MODULE ${_abs_sources})
  endif()

  # --- isolated compile environment (both modes) ----------------------------------
  # The abi include dir directly (not QtClientSdk::Abi): a STATIC_ABI export must not
  # name an SDK target the host build does not define.
  get_target_property(_abi_inc QtClientSdk::Abi INTERFACE_INCLUDE_DIRECTORIES)
  target_include_directories(${_mod_target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" ${_abi_inc})
  target_compile_definitions(${_mod_target} PRIVATE
    QTCLIENT_MOD_BUILD=1
    QTCLIENT_MOD_ID=${MOD_ID}
    QTCLIENT_MOD_C_ID=${_c_id}
    QTCLIENT_PACK_ABI=1
    QT_NO_KEYWORDS)
  if(_mode STREQUAL "STATIC_ABI")
    target_compile_definitions(${_mod_target} PRIVATE QTCLIENT_MOD_STATIC_ABI=1)
  endif()
  if(ARG_GUI_PRIVATE)
    target_compile_definitions(${_mod_target} PRIVATE QTCLIENT_MOD_GUI_PRIVATE=1)
  endif()
  if(NOT DEFINED CMAKE_CXX_STANDARD)
    target_compile_features(${_mod_target} PRIVATE cxx_std_20)
  endif()
  set_target_properties(${_mod_target} PROPERTIES
    AUTOMOC ON
    AUTORCC ON
    CXX_VISIBILITY_PRESET hidden
    C_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    POSITION_INDEPENDENT_CODE ON)

  if(NOT ARG_NO_QT)
    target_link_libraries(${_mod_target} PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)
    if(ARG_GUI_PRIVATE)
      target_link_libraries(${_mod_target} PRIVATE Qt6::GuiPrivate Qt6::QuickPrivate)
    endif()
  endif()

  # --- MODULE: shared library loaded by the tip -----------------------------------
  set(_mod_suffix "")
  if(_mode STREQUAL "MODULE")
    if(EMSCRIPTEN OR WASM)
      set(_mod_suffix ".wasm")
    elseif(WIN32)
      set(_mod_suffix ".dll")
    elseif(APPLE)
      set(_mod_suffix ".dylib")
    else()
      set(_mod_suffix ".so")
    endif()
    set_target_properties(${_mod_target} PROPERTIES
      PREFIX ""
      OUTPUT_NAME "mod"
      SUFFIX "${_mod_suffix}"
      LIBRARY_OUTPUT_DIRECTORY "${_outdir}/native"
      RUNTIME_OUTPUT_DIRECTORY "${_outdir}/native"
      LIBRARY_OUTPUT_DIRECTORY_DEBUG "${_outdir}/native"
      LIBRARY_OUTPUT_DIRECTORY_RELEASE "${_outdir}/native"
      LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${_outdir}/native"
      RUNTIME_OUTPUT_DIRECTORY_DEBUG "${_outdir}/native"
      RUNTIME_OUTPUT_DIRECTORY_RELEASE "${_outdir}/native"
      RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${_outdir}/native")
    if(EMSCRIPTEN OR WASM)
      # Only NO_QT packs get here (a Qt pack on the static wasm kit is STATIC_ABI).
      target_link_options(${_mod_target} PRIVATE
        "SHELL:-sSIDE_MODULE=2"
        "SHELL:-sERROR_ON_UNDEFINED_SYMBOLS=0")
    elseif(APPLE)
      # Tip layout: <App>.app/Contents/MacOS/mods/<id>/native/mod.dylib.
      # Bundled Qt (macdeployqt) first, then the kit for build-tree runs; the
      # packager drops the kit path (tools/publish/package_desktop.py).
      set(_rpath "@loader_path/../../../../Frameworks")
      if(TARGET Qt6::Core)
        get_target_property(_qt_core_loc Qt6::Core LOCATION)
        if(_qt_core_loc MATCHES "^(.*)/QtCore\\.framework/")
          list(APPEND _rpath "${CMAKE_MATCH_1}")
        endif()
      endif()
      set_target_properties(${_mod_target} PROPERTIES
        BUILD_WITH_INSTALL_RPATH ON
        INSTALL_RPATH "${_rpath}")
      target_link_options(${_mod_target} PRIVATE "LINKER:-dead_strip")
    elseif(UNIX)
      # Tip layout: <bin>/mods/<id>/native/mod.so with Qt in <bin>/lib.
      # --no-undefined: a symbol only the tip could provide fails the link.
      set_target_properties(${_mod_target} PROPERTIES
        INSTALL_RPATH "\$ORIGIN/../../../lib")
      target_link_options(${_mod_target} PRIVATE "LINKER:--no-undefined" "LINKER:--as-needed")
    endif()
    # Windows: the Qt DLLs are already loaded by name from the tip's directory.
  endif()

  set_property(GLOBAL PROPERTY QTCLIENT_MOD_${_c_id}_MODE "${_mode}")
  set_property(GLOBAL PROPERTY QTCLIENT_MOD_${_c_id}_STAGE "${_outdir}")
  set_property(GLOBAL PROPERTY QTCLIENT_MOD_${_c_id}_GUI_PRIVATE "${ARG_GUI_PRIVATE}")
  set_property(GLOBAL PROPERTY QTCLIENT_MOD_${_c_id}_NO_QT "${ARG_NO_QT}")

  if(NOT TARGET qtclient_mods)
    add_custom_target(qtclient_mods ALL COMMENT "qtclient packs")
  endif()
  add_dependencies(qtclient_mods ${_mod_target})
  if(_mode STREQUAL "STATIC_ABI")
    message(STATUS "qtclient_add_mod: ${MOD_ID} → STATIC_ABI QtClientPack::${_c_id} (qtclient_mod_${_c_id}_register_abi)")
  else()
    message(STATUS "qtclient_add_mod: ${MOD_ID} → MODULE ${_outdir}/native/mod${_mod_suffix} (qtclient_mod_register)")
  endif()
endfunction()

function(qtclient_mod_install MOD_ID)
  cmake_parse_arguments(ARG "" "" "EXTRA_TARGETS;BRAND_FILES" ${ARGN})
  string(MAKE_C_IDENTIFIER "${MOD_ID}" _c_id)
  set(_mod_target "qtclient_mod_${MOD_ID}")
  if(NOT TARGET ${_mod_target})
    message(FATAL_ERROR "qtclient_mod_install(${MOD_ID}): call qtclient_add_mod(${MOD_ID}) first")
  endif()
  get_property(_mode GLOBAL PROPERTY QTCLIENT_MOD_${_c_id}_MODE)
  get_property(_outdir GLOBAL PROPERTY QTCLIENT_MOD_${_c_id}_STAGE)
  get_property(_gui_private GLOBAL PROPERTY QTCLIENT_MOD_${_c_id}_GUI_PRIVATE)
  get_property(_no_qt GLOBAL PROPERTY QTCLIENT_MOD_${_c_id}_NO_QT)

  # FS tree (configure-time stage + whatever the pack copied into it).
  install(DIRECTORY "${_outdir}/" DESTINATION "mods/${MOD_ID}"
    PATTERN "native" EXCLUDE)

  if(_mode STREQUAL "MODULE")
    install(TARGETS ${_mod_target}
      LIBRARY DESTINATION "mods/${MOD_ID}/native"
      RUNTIME DESTINATION "mods/${MOD_ID}/native")
  else()
    set(_cfg_dir "lib/cmake/QtClientPack_${_c_id}")
    install(TARGETS ${_mod_target} ${ARG_EXTRA_TARGETS}
      EXPORT QtClientPack_${_c_id}Targets
      ARCHIVE DESTINATION lib
      LIBRARY DESTINATION lib
      OBJECTS DESTINATION "lib/objects-${MOD_ID}")
    install(EXPORT QtClientPack_${_c_id}Targets
      NAMESPACE QtClientPack::
      DESTINATION "${_cfg_dir}")
    set(_qt_components "Core Gui Qml Quick")
    if(_gui_private)
      string(APPEND _qt_components " GuiPrivate QuickPrivate")
    endif()
    set(_find_qt "")
    if(NOT _no_qt)
      set(_find_qt "find_dependency(Qt6 COMPONENTS ${_qt_components})\n")
    endif()
    set(_cfg "${CMAKE_CURRENT_BINARY_DIR}/QtClientPack_${_c_id}Config.cmake")
    file(WRITE "${_cfg}" "# Generated by qtclient_mod_install — static-ABI pack ${MOD_ID} for the host build.
include(CMakeFindDependencyMacro)
${_find_qt}include(\"\${CMAKE_CURRENT_LIST_DIR}/QtClientPack_${_c_id}Targets.cmake\")
set(QTCLIENT_PACK_${_c_id}_ID \"${MOD_ID}\")
set(QTCLIENT_PACK_${_c_id}_TARGET QtClientPack::${_c_id})
set(QTCLIENT_PACK_${_c_id}_HOST_BUILD_ID \"${QTCLIENT_SDK_HOST_BUILD_ID}\")
set(QTCLIENT_PACK_${_c_id}_QT_VERSION \"${QTCLIENT_SDK_QT_VERSION}\")
")
    install(FILES "${_cfg}" DESTINATION "${_cfg_dir}")
  endif()

  if(ARG_BRAND_FILES)
    install(FILES ${ARG_BRAND_FILES} DESTINATION "share/qtclient-pack/${MOD_ID}")
  endif()
endfunction()
