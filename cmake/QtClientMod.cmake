# qtclient_add_mod — pack MODULE (host-pushed C ABI) or deprecated tip absorb.
#
#   qtclient_add_mod(<id>
#     SOURCES …
#     [QML_DIR dir]
#     [OVERRIDES_DIR dir]
#     [RESOURCES_DIR dir]
#     [MOD_JSON path]
#     [QML_MODULE name]
#     [ABSORB_INTO_QTCLIENT]   # deprecated: compile into tibia (IdleOT peel WIP)
#   )
#
# Default (runtime MODULE):
#   mods/<id>/mod.json
#   mods/<id>/native/mod.<ext>   # exports qtclient_mod_register
#   mods/<id>/qml/ overrides/ resources/
#
# Pack TUs compile against include/qtclient/abi/** only (zero tip imports).
# Tip ModLoader dlopens native/mod.* and passes QtClientHostApiV1*.

include_guard(GLOBAL)

function(qtclient_add_mod MOD_ID)
  cmake_parse_arguments(ARG "ABSORB_INTO_QTCLIENT" "QML_DIR;OVERRIDES_DIR;RESOURCES_DIR;MOD_JSON;QML_MODULE" "SOURCES" ${ARGN})
  if(NOT ARG_SOURCES)
    message(FATAL_ERROR "qtclient_add_mod(${MOD_ID}): SOURCES required")
  endif()
  if(NOT ARG_QML_MODULE)
    set(ARG_QML_MODULE "${MOD_ID}")
  endif()
  if(NOT TARGET tibia)
    message(FATAL_ERROR "qtclient_add_mod(${MOD_ID}): tibia target missing")
  endif()

  set(_outdir "${CMAKE_BINARY_DIR}/mods/${MOD_ID}")
  file(MAKE_DIRECTORY "${_outdir}")
  # native/ is created only for runtime MODULE packs (below).

  set(_abs_sources "")
  foreach(_src IN LISTS ARG_SOURCES)
    if(IS_ABSOLUTE "${_src}")
      list(APPEND _abs_sources "${_src}")
    else()
      list(APPEND _abs_sources "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
    endif()
  endforeach()

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

  # FS chrome
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
    file(COPY "${_qml_stage_from}/" DESTINATION "${_outdir}/qml")
    execute_process(
      COMMAND ${CMAKE_COMMAND}
        -DMODULE_DIR=${_outdir}/qml/${ARG_QML_MODULE}
        -DMODULE_NAME=${ARG_QML_MODULE}
        -P "${TIBIA_ROOT}/cmake/GeneratePackQmldir.cmake"
      RESULT_VARIABLE _qmldir_rc)
    if(NOT _qmldir_rc EQUAL 0)
      message(WARNING "qtclient_add_mod(${MOD_ID}): GeneratePackQmldir failed (${_qmldir_rc})")
    endif()
  endif()
  if(ARG_OVERRIDES_DIR AND EXISTS "${ARG_OVERRIDES_DIR}")
    file(COPY "${ARG_OVERRIDES_DIR}/" DESTINATION "${_outdir}/overrides")
  endif()
  set(_resources_dir "")
  if(ARG_RESOURCES_DIR AND EXISTS "${ARG_RESOURCES_DIR}")
    file(COPY "${ARG_RESOURCES_DIR}/" DESTINATION "${_outdir}/resources")
    set(_resources_dir "${ARG_RESOURCES_DIR}")
  endif()

  if(ARG_ABSORB_INTO_QTCLIENT)
    message(WARNING "qtclient_add_mod(${MOD_ID}): ABSORB_INTO_QTCLIENT is deprecated — migrate to runtime MODULE + abi/")
    # Absorb packs are tip-linked C++; never stage an empty native/ tree.
    file(REMOVE_RECURSE "${_outdir}/native")
    target_sources(tibia PRIVATE ${_abs_sources})
    target_include_directories(tibia PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    if(QTCLIENT_PACK_SDK_ONLY)
      if(QTCLIENT_HOST_INCLUDE_DIR)
        target_include_directories(tibia PRIVATE ${QTCLIENT_HOST_INCLUDE_DIR})
      else()
        target_include_directories(tibia PRIVATE
          "${TIBIA_ROOT}/include"
          "${TIBIA_ROOT}/client"
          "${TIBIA_ROOT}/client/tibia")
        if(DEFINED CMAKE_BINARY_DIR)
          target_include_directories(tibia PRIVATE
            "${CMAKE_BINARY_DIR}/client/tibia/cipgen"
            "${CMAKE_BINARY_DIR}/client/tibia/protobuf"
            "${CMAKE_BINARY_DIR}/client/tibia/tibia_autogen/include")
        endif()
      endif()
    else()
      target_include_directories(tibia PRIVATE
        "${TIBIA_ROOT}/include"
        "${TIBIA_ROOT}/client/tibia")
      if(QTCLIENT_HOST_INCLUDE_DIR)
        target_include_directories(tibia PRIVATE ${QTCLIENT_HOST_INCLUDE_DIR})
      endif()
    endif()
    # ABSORB skips qtclient_mod_register (MODULE-only); tip uses qtclient_mod_<id>_register.
    set(_pack_defs "QTCLIENT_MOD_BUILD=1;QTCLIENT_MOD_ID=${MOD_ID};QTCLIENT_ABSORB_INTO_QTCLIENT=1")
    if(QTCLIENT_MOD_INTERNAL)
      list(APPEND _pack_defs "QTCLIENT_MOD_INTERNAL=1")
    endif()
    if(QTCLIENT_MOD_GUI_PRIVATE)
      list(APPEND _pack_defs "QTCLIENT_MOD_GUI_PRIVATE=1")
    endif()
    set_property(SOURCE ${_abs_sources} TARGET_DIRECTORY tibia
      APPEND PROPERTY COMPILE_DEFINITIONS ${_pack_defs})
    set(_pack_cpp ${_abs_sources})
    list(FILTER _pack_cpp INCLUDE REGEX "\\.cpp$")
    if(_pack_cpp)
      if(MSVC)
        # Direct /FI of containerciptypes (was framework/pack_msvc_ns_anchor.h).
        set_property(SOURCE ${_pack_cpp} TARGET_DIRECTORY tibia
          APPEND PROPERTY COMPILE_OPTIONS
            "/FIframework/pch.h"
            "/FIcontainer/containerciptypes.h")
      else()
        set_property(SOURCE ${_pack_cpp} TARGET_DIRECTORY tibia
          APPEND PROPERTY COMPILE_OPTIONS "-include" "framework/pch.h")
      endif()
    endif()
    if(TARGET Qt6::Core AND (QTCLIENT_MOD_GUI_PRIVATE OR QTCLIENT_MOD_INTERNAL))
      if(TARGET Qt6::GuiPrivate)
        target_link_libraries(tibia PRIVATE Qt6::GuiPrivate)
      endif()
      if(TARGET Qt6::QuickPrivate)
        target_link_libraries(tibia PRIVATE Qt6::QuickPrivate)
      endif()
    endif()
    if(TARGET cip_codegen_tibia)
      add_dependencies(tibia cip_codegen_tibia)
    endif()
    set_property(GLOBAL APPEND PROPERTY QTCLIENT_LINKED_MOD_IDS "${MOD_ID}")
    set_property(GLOBAL APPEND PROPERTY QTCLIENT_MOD_TIP_STAGE
      "${MOD_ID}|${_outdir}|${_resources_dir}|")
    if(NOT TARGET qtclient_mods)
      add_custom_target(qtclient_mods COMMENT "qtclient packs")
    endif()
    add_dependencies(qtclient_mods tibia)
    message(STATUS "qtclient_add_mod: ${MOD_ID} ABSORB → tibia (deprecated)")
    return()
  endif()

  # --- Runtime MODULE path ---
  file(MAKE_DIRECTORY "${_outdir}/native")
  set(_mod_target "qtclient_mod_${MOD_ID}")
  add_library(${_mod_target} MODULE ${_abs_sources})
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
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${_outdir}/native"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${_outdir}/native"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    POSITION_INDEPENDENT_CODE ON)

  target_include_directories(${_mod_target} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${TIBIA_ROOT}/include")
  target_compile_definitions(${_mod_target} PRIVATE
    QTCLIENT_MOD_BUILD=1
    QTCLIENT_MOD_ID=${MOD_ID}
    QTCLIENT_PACK_ABI=1)
  # ABI-only: no tip pch, no tip sources.
  if(EMSCRIPTEN OR WASM)
    # SIDE_MODULE with zero tip imports; host dlopens and calls qtclient_mod_register.
    target_link_options(${_mod_target} PRIVATE
      "SHELL:-sSIDE_MODULE=2"
      "SHELL:-sERROR_ON_UNDEFINED_SYMBOLS=0")
  endif()

  # Stage tip tree after both tip and MODULE build.
  set_property(GLOBAL APPEND PROPERTY QTCLIENT_MOD_TIP_STAGE
    "${MOD_ID}|${_outdir}|${_resources_dir}|${_mod_target}")
  set_property(GLOBAL APPEND PROPERTY QTCLIENT_RUNTIME_MOD_IDS "${MOD_ID}")

  if(NOT TARGET qtclient_mods)
    add_custom_target(qtclient_mods COMMENT "qtclient packs")
  endif()
  add_dependencies(qtclient_mods ${_mod_target})
  add_dependencies(tibia ${_mod_target})
  message(STATUS "qtclient_add_mod: ${MOD_ID} → MODULE ${_outdir}/native/mod.* (qtclient_mod_register)")
endfunction()

# Deprecated archive link (absorb). Prefer runtime MODULE artifacts.
function(qtclient_link_mod_archive MOD_ID)
  cmake_parse_arguments(ARG "" "ARCHIVE;MOD_DIR" "" ${ARGN})
  if(NOT ARG_ARCHIVE OR NOT EXISTS "${ARG_ARCHIVE}")
    message(FATAL_ERROR "qtclient_link_mod_archive(${MOD_ID}): ARCHIVE missing")
  endif()
  if(NOT TARGET tibia)
    message(FATAL_ERROR "qtclient_link_mod_archive(${MOD_ID}): tibia target missing")
  endif()
  message(WARNING "qtclient_link_mod_archive(${MOD_ID}): deprecated — ship runtime MODULE under mods/<id>/native/")
  add_library(qtclient_mod_${MOD_ID} STATIC IMPORTED GLOBAL)
  set_target_properties(qtclient_mod_${MOD_ID} PROPERTIES
    IMPORTED_LOCATION "${ARG_ARCHIVE}")
  target_link_libraries(tibia PRIVATE qtclient_mod_${MOD_ID})
  set_property(GLOBAL APPEND PROPERTY QTCLIENT_LINKED_MOD_IDS "${MOD_ID}")
  if(ARG_MOD_DIR AND EXISTS "${ARG_MOD_DIR}/mod.json")
    set_property(GLOBAL APPEND PROPERTY QTCLIENT_MOD_TIP_STAGE
      "${MOD_ID}|${ARG_MOD_DIR}||")
  endif()
endfunction()
