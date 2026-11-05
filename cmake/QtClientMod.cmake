# qtclient_add_mod — STATIC pack linked into the qtclient tip (desktop + WASM).
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
# Pack C++ is STATIC-linked into `tibia`. Each pack exports
# qtclient_mod_<id>_entry; tibia_finalize_linked_mods generates
# qtclient_invoke_linked_mods. QML stays filesystem for F5.
#
# Optional prebuilt archive (Phase 3):
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

  set(_target "qtclient_mod_${MOD_ID}")
  set(_outdir "${CMAKE_BINARY_DIR}/mods/${MOD_ID}")
  file(MAKE_DIRECTORY "${_outdir}")

  add_library(${_target} STATIC ${ARG_SOURCES})
  set_target_properties(${_target} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${_outdir}/lib"
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME "qtclient_mod_${MOD_ID}")

  target_compile_definitions(${_target} PRIVATE
    QTCLIENT_MOD_BUILD=1
    QTCLIENT_MOD_ID=${MOD_ID})
  # Always see the pack's own sources.
  target_include_directories(${_target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

  # Phase 2 SDK gate: curated roots only (include/qtclient + tip closure + engine
  # for framework/ + cipgen + protobuf). No full tip INCLUDE_DIRECTORIES dump.
  if(QTCLIENT_PACK_SDK_ONLY)
    if(QTCLIENT_HOST_INCLUDE_DIR)
      target_include_directories(${_target} PRIVATE ${QTCLIENT_HOST_INCLUDE_DIR})
    else()
      # In-tree linked tip: synthesize pack_sdk roots from TIBIA_ROOT + tip codegen.
      # client/ is required for <tibia/…> includes (maps to client/tibia/…).
      target_include_directories(${_target} PRIVATE
        "${TIBIA_ROOT}/include"
        "${TIBIA_ROOT}/client"
        "${TIBIA_ROOT}/client/tibia"
        "${TIBIA_ROOT}/client/tibia/engine")
      if(DEFINED CMAKE_BINARY_DIR)
        target_include_directories(${_target} PRIVATE
          "${CMAKE_BINARY_DIR}/client/tibia/cipgen"
          "${CMAKE_BINARY_DIR}/client/tibia/protobuf"
          "${CMAKE_BINARY_DIR}/client/tibia/tibia_autogen/include")
      endif()
    endif()
  else()
    target_include_directories(${_target} PRIVATE
      "${TIBIA_ROOT}/include"
      "${TIBIA_ROOT}/client/tibia")
    # Tip include dirs imply tip keyword/defs (QT_NO_KEYWORDS, …).
    target_include_directories(${_target} PRIVATE
      "$<TARGET_PROPERTY:tibia,INCLUDE_DIRECTORIES>")
    if(QTCLIENT_HOST_INCLUDE_DIR)
      target_include_directories(${_target} PRIVATE ${QTCLIENT_HOST_INCLUDE_DIR})
    endif()
  endif()
  target_compile_definitions(${_target} PRIVATE
    "$<TARGET_PROPERTY:tibia,COMPILE_DEFINITIONS>")
  target_compile_options(${_target} PRIVATE
    "$<TARGET_PROPERTY:tibia,COMPILE_OPTIONS>")
  # MSVC pack TUs: force-include a tibia nest anchor before any pack/tip header.
  # Without this, sibling `tibia::container` lookup fails (C3083) after the
  # appearanceinstance nest — tip engine TUs avoid it via /FI framework/pch.h
  # plus a different include graph; packs do not.
  if(MSVC)
    target_compile_options(${_target} PRIVATE "/FIframework/pack_msvc_ns_anchor.h")
  endif()
  # PACK_SDK_ONLY curated roots omit tip LINK_LIBRARIES. Engine headers on that
  # include path (and tip PCH) still need zlib/lzma/spdlog includes — on Windows
  # vcpkg those are not on the default include path (linux/mac system zlib often is).
  if(TARGET tibia_thirdparty)
    target_link_libraries(${_target} PRIVATE tibia_thirdparty)
  elseif(TARGET ZLIB::ZLIB)
    target_link_libraries(${_target} PRIVATE ZLIB::ZLIB)
  endif()
  # qtclient headers (e.g. eventfilters.h → <cip_gen.h>) need codegen before the
  # pack compiles; include dirs alone do not order the custom command.
  if(TARGET cip_codegen_tibia)
    add_dependencies(${_target} cip_codegen_tibia)
  endif()
  # QTCLIENT_MOD_INTERNAL=1 define only — for any remaining #ifdef; not a defs gate.
  if(QTCLIENT_MOD_INTERNAL)
    target_compile_definitions(${_target} PRIVATE QTCLIENT_MOD_INTERNAL=1)
  endif()
  # Narrow RHI / private Qt link — independent of deep tip includes.
  # Packs that only need GuiPrivate for weather/raycast set QTCLIENT_MOD_GUI_PRIVATE
  # without QTCLIENT_MOD_INTERNAL once deep headers are gone.
  if(QTCLIENT_MOD_GUI_PRIVATE)
    target_compile_definitions(${_target} PRIVATE QTCLIENT_MOD_GUI_PRIVATE=1)
  endif()

  if(TARGET Qt6::Core)
    target_link_libraries(${_target} PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)
    if((QTCLIENT_MOD_GUI_PRIVATE OR QTCLIENT_MOD_INTERNAL) AND TARGET Qt6::GuiPrivate)
      target_link_libraries(${_target} PRIVATE Qt6::GuiPrivate)
    endif()
    if((QTCLIENT_MOD_GUI_PRIVATE OR QTCLIENT_MOD_INTERNAL) AND TARGET Qt6::QuickPrivate)
      target_link_libraries(${_target} PRIVATE Qt6::QuickPrivate)
    endif()
  endif()

  set_target_properties(${_target} PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    AUTOMOC ON)

  # Tip links the pack; finalize step defines QTCLIENT_HAS_LINKED_PACK + entry table.
  target_link_libraries(tibia PRIVATE ${_target})
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
    add_custom_command(TARGET ${_target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E rm -rf "${_outdir}/qml"
      COMMAND ${CMAKE_COMMAND} -E copy_directory "${_qml_stage_from}" "${_outdir}/qml"
      COMMAND ${CMAKE_COMMAND}
        -DMODULE_DIR="${_outdir}/qml/${ARG_QML_MODULE}"
        -DMODULE_NAME="${ARG_QML_MODULE}"
        -P "${TIBIA_ROOT}/cmake/GeneratePackQmldir.cmake"
      COMMENT "stage ${MOD_ID} qml FS module ${ARG_QML_MODULE}")
  endif()
  if(ARG_OVERRIDES_DIR AND EXISTS "${ARG_OVERRIDES_DIR}")
    add_custom_command(TARGET ${_target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory "${ARG_OVERRIDES_DIR}" "${_outdir}/overrides"
      COMMENT "stage ${MOD_ID} overrides")
  endif()
  if(ARG_RESOURCES_DIR AND EXISTS "${ARG_RESOURCES_DIR}")
    add_custom_command(TARGET ${_target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory "${ARG_RESOURCES_DIR}" "${_outdir}/resources"
      COMMENT "stage ${MOD_ID} resources")
  endif()

  # Defer tip FS staging: add_custom_command(TARGET tibia) only works in the
  # directory that created `tibia` (mods/ is a subdirectory).
  set_property(GLOBAL APPEND PROPERTY QTCLIENT_MOD_TIP_STAGE
    "${MOD_ID}|${_outdir}")

  set(QTCLIENT_MOD_TARGET_${MOD_ID} "${_target}" PARENT_SCOPE)
  if(NOT TARGET qtclient_mods)
    add_custom_target(qtclient_mods COMMENT "all qtclient linked STATIC packs")
  endif()
  add_dependencies(qtclient_mods ${_target})
  message(STATUS "qtclient_add_mod: ${MOD_ID} STATIC → tibia + ${_outdir} (entry qtclient_mod_${MOD_ID}_entry)")
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
    add_custom_target(qtclient_mods COMMENT "all qtclient linked STATIC packs")
  endif()
  message(STATUS "qtclient_link_mod_archive: ${MOD_ID} ← ${ARG_ARCHIVE}")
endfunction()
