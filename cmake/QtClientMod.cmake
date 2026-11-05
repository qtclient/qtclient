# qtclient_add_mod — pack C++ absorbed into the `tibia` tip (CIP-shaped).
#
#   qtclient_add_mod(<id>
#     SOURCES …
#     [QML_DIR dir]
#     [OVERRIDES_DIR dir]
#     [RESOURCES_DIR dir]
#     [MOD_JSON path]
#     [QML_MODULE name]
#   )
#
# Stages (no native/SIDE_MODULE ship path):
#   mods/<id>/mod.json
#   mods/<id>/qml/<qmlModule>/…
#   mods/<id>/overrides/ …
#   mods/<id>/resources/ …
#
# Pack C++ is compiled as part of `tibia` (same /FI framework/pch.h preamble as
# tip engine TUs — CipSoft monolithic client). Pack TUs export
# `qtclient_mod_<id>_register` (C++). Tip-generated linked_mods.cpp owns the
# only `extern "C"` wrappers + qtclient_invoke_linked_mods. QML stays FS for F5.
#
# Optional prebuilt archive (Phase 3, IMPORTED):
#   -DQTCLIENT_LINKED_PACK_LIBS=<id>=/path/to/libqtclient_mod_<id>.a;…

include_guard(GLOBAL)

function(qtclient_add_mod MOD_ID)
  cmake_parse_arguments(ARG "" "QML_DIR;OVERRIDES_DIR;RESOURCES_DIR;MOD_JSON;QML_MODULE" "SOURCES" ${ARGN})
  if(NOT ARG_SOURCES)
    message(FATAL_ERROR "qtclient_add_mod(${MOD_ID}): SOURCES required")
  endif()
  if(NOT ARG_QML_MODULE)
    set(ARG_QML_MODULE "${MOD_ID}")
  endif()
  if(NOT TARGET tibia)
    message(FATAL_ERROR "qtclient_add_mod(${MOD_ID}): tibia target missing (linked tip)")
  endif()

  set(_outdir "${CMAKE_BINARY_DIR}/mods/${MOD_ID}")
  file(MAKE_DIRECTORY "${_outdir}")

  # Resolve sources to absolute paths for set_source_files_properties.
  set(_abs_sources "")
  foreach(_src IN LISTS ARG_SOURCES)
    if(IS_ABSOLUTE "${_src}")
      list(APPEND _abs_sources "${_src}")
    else()
      list(APPEND _abs_sources "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
    endif()
  endforeach()

  target_sources(tibia PRIVATE ${_abs_sources})

  # Pack-local includes + SDK roots on the tip (sources compile as tibia TUs).
  target_include_directories(tibia PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
  if(QTCLIENT_PACK_SDK_ONLY)
    if(QTCLIENT_HOST_INCLUDE_DIR)
      target_include_directories(tibia PRIVATE ${QTCLIENT_HOST_INCLUDE_DIR})
    else()
      target_include_directories(tibia PRIVATE
        "${TIBIA_ROOT}/include"
        "${TIBIA_ROOT}/client"
        "${TIBIA_ROOT}/client/tibia"
        "${TIBIA_ROOT}/client/tibia/engine")
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

  # Per-pack defs (do not pollute the whole tip with QTCLIENT_MOD_ID).
  # TARGET_DIRECTORY tibia is required: qtclient_add_mod runs from the pack's
  # CMakeLists (e.g. mods/_sample or out-of-tree IdleOT), not the directory that
  # created `tibia`. Without it, Ninja ignores SOURCE COMPILE_* properties and
  # pack TUs miss /FI pch + QTCLIENT_MOD_* (MSVC C3083 on tibia::container).
  set(_pack_defs "QTCLIENT_MOD_BUILD=1;QTCLIENT_MOD_ID=${MOD_ID}")
  if(QTCLIENT_MOD_INTERNAL)
    list(APPEND _pack_defs "QTCLIENT_MOD_INTERNAL=1")
  endif()
  if(QTCLIENT_MOD_GUI_PRIVATE)
    list(APPEND _pack_defs "QTCLIENT_MOD_GUI_PRIVATE=1")
  endif()
  set_property(SOURCE ${_abs_sources} TARGET_DIRECTORY tibia
    APPEND PROPERTY COMPILE_DEFINITIONS ${_pack_defs})

  # Same force-include as tip engine .cpp (client/tibia/CMakeLists.txt), plus
  # MSVC pack anchor that pre-parses containerciptypes (pch alone is not enough
  # for session/game hubs — C3083 on ::tibia::container::TInventorySlot).
  set(_pack_cpp ${_abs_sources})
  list(FILTER _pack_cpp INCLUDE REGEX "\\.cpp$")
  if(_pack_cpp)
    if(MSVC)
      set_property(SOURCE ${_pack_cpp} TARGET_DIRECTORY tibia
        APPEND PROPERTY COMPILE_OPTIONS
          "/FIframework/pch.h"
          "/FIframework/pack_msvc_ns_anchor.h")
    else()
      set_property(SOURCE ${_pack_cpp} TARGET_DIRECTORY tibia
        APPEND PROPERTY COMPILE_OPTIONS "-include" "framework/pch.h")
    endif()
  endif()

  # GuiPrivate already linked on tibia when product needs it — ensure tip has it.
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

  # FS chrome at configure time (no separate pack target POST_BUILD).
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
  if(ARG_RESOURCES_DIR AND EXISTS "${ARG_RESOURCES_DIR}")
    file(COPY "${ARG_RESOURCES_DIR}/" DESTINATION "${_outdir}/resources")
  endif()

  # Defer tip FS staging: add_custom_command(TARGET tibia) only works in the
  # directory that created `tibia` (mods/ is a subdirectory).
  set_property(GLOBAL APPEND PROPERTY QTCLIENT_MOD_TIP_STAGE
    "${MOD_ID}|${_outdir}")

  if(NOT TARGET qtclient_mods)
    add_custom_target(qtclient_mods COMMENT "qtclient linked packs (absorbed into tibia)")
  endif()
  add_dependencies(qtclient_mods tibia)
  message(STATUS "qtclient_add_mod: ${MOD_ID} → tibia sources + ${_outdir} (register qtclient_mod_${MOD_ID}_register)")
endfunction()

# Link a prebuilt pack archive (Phase 3) without compiling pack sources in-tree.
#   qtclient_link_mod_archive(<id> ARCHIVE /path/to/libqtclient_mod_<id>.a
#     [MOD_DIR /path/to/staged/mods/<id>])
function(qtclient_link_mod_archive MOD_ID)
  cmake_parse_arguments(ARG "" "ARCHIVE;MOD_DIR" "" ${ARGN})
  if(NOT ARG_ARCHIVE OR NOT EXISTS "${ARG_ARCHIVE}")
    message(FATAL_ERROR "qtclient_link_mod_archive(${MOD_ID}): ARCHIVE missing")
  endif()
  if(NOT TARGET tibia)
    message(FATAL_ERROR "qtclient_link_mod_archive(${MOD_ID}): tibia target missing")
  endif()
  add_library(qtclient_mod_${MOD_ID} STATIC IMPORTED GLOBAL)
  set_target_properties(qtclient_mod_${MOD_ID} PROPERTIES
    IMPORTED_LOCATION "${ARG_ARCHIVE}")
  target_link_libraries(tibia PRIVATE qtclient_mod_${MOD_ID})
  set_property(GLOBAL APPEND PROPERTY QTCLIENT_LINKED_MOD_IDS "${MOD_ID}")
  if(ARG_MOD_DIR AND EXISTS "${ARG_MOD_DIR}/mod.json")
    set_property(GLOBAL APPEND PROPERTY QTCLIENT_MOD_TIP_STAGE
      "${MOD_ID}|${ARG_MOD_DIR}")
  endif()
  if(NOT TARGET qtclient_mods)
    add_custom_target(qtclient_mods COMMENT "qtclient linked packs (absorbed into tibia)")
  endif()
  message(STATUS "qtclient_link_mod_archive: ${MOD_ID} ← ${ARG_ARCHIVE}")
endfunction()
