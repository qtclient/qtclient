# CIP's qmlcomponents module. The official client builds this with qt_add_qml_module
# (one qmlcache unit per file, qmldir generated at build time). upstream/ is vendored
# and has no CMake of its own; this function feeds that tree in.
#
# Prefer leaving CIP QML alone. Overlay layers (later wins):
#   client/overrides/qmlcomponents/<rel>
#   client/mods/<enabled>/overrides/qmlcomponents/<rel>
# Product UI that is not a stock-path fork belongs in client/mods/<pack>/qml.
#
# Always stage QML into the build tree (qml-embed/) so qmlcachegen never emits
# …/qmlcomponents_../upstream paths that ninja cannot mkdir on Windows.
# TIBIA_QML_DISK: NO_CACHEGEN + non-CONFIGURE_DEPENDS glob; DiskQmlInterceptor
# hot-reloads qtclient + overrides only — qmlcomponents stays on qrc.

function(_cip_write_if_changed path content)
  if(EXISTS "${path}")
    file(READ "${path}" current)
    if(current STREQUAL content)
      return()
    endif()
  endif()
  file(WRITE "${path}" "${content}")
endfunction()

# Collect override roots for configure-time qmlcomponents merge.
# Engine client/overrides only — pack overrides/ are runtime (PackQmlInterceptor)
# so stock hosts publish once and product packs overlay without rebuilding.
# Sets CIP_QML_OVERRIDE_ROOTS in parent scope (qmlcomponents parent dirs).
function(cip_collect_qml_override_roots)
  set(roots "")
  set(_engine "${CIP_ROOT}/client/overrides/qmlcomponents")
  if(EXISTS "${_engine}")
    list(APPEND roots "${_engine}")
  endif()
  set(CIP_QML_OVERRIDE_ROOTS "${roots}" PARENT_SCOPE)
endfunction()

function(_cip_resolve_override rel out_var)
  set(found "")
  foreach(root IN LISTS CIP_QML_OVERRIDE_ROOTS)
    set(candidate "${root}/${rel}")
    if(EXISTS "${candidate}")
      set(found "${candidate}")
    endif()
  endforeach()
  set(${out_var} "${found}" PARENT_SCOPE)
endfunction()

function(cip_qml_module)
  set(root "${CIP_ROOT}/client/upstream/qml/qmlcomponents")
  if(NOT EXISTS "${root}")
    message(FATAL_ERROR "cip qmlcomponents tree missing: ${root}")
  endif()

  cip_collect_qml_override_roots()
  if(CIP_QML_OVERRIDE_ROOTS)
    message(STATUS "cip qml overrides: ${CIP_QML_OVERRIDE_ROOTS}")
  endif()

  if(TIBIA_QML_DISK)
    file(GLOB_RECURSE cip_files "${root}/*")
  else()
    file(GLOB_RECURSE cip_files CONFIGURE_DEPENDS "${root}/*")
  endif()

  set(qml_files)
  set(resources)
  set(singleton_lines)
  set(dir_entries)
  set(embed_root "${CMAKE_CURRENT_BINARY_DIR}/qml-embed/qmlcomponents")
  foreach(file IN LISTS cip_files)
    file(RELATIVE_PATH rel "${root}" "${file}")
    # Qt writes the module qmldir. Directory qmldirs are generated below so a
    # subdirectory import still sees its sibling types. CIP's vendored copies
    # are only a `prefer` line, which hides those siblings.
    if(rel MATCHES "(^|/)qmldir$")
      continue()
    endif()
    if(NOT rel MATCHES "\\.(qml|js)$")
      continue()
    endif()

    set(src "${file}")
    _cip_resolve_override("${rel}" _override)
    if(_override)
      set(src "${_override}")
    endif()

    # Sources live under client/upstream (or overrides/mods), outside
    # client/tibia. Qt's qmlcachegen names cache dirs from the relative path
    # → …/qmlcomponents_../upstream/… (or _../mods/…) which ninja cannot mkdir
    # on Windows. Always stage into the build tree first.
    set(stub "${embed_root}/${rel}")
    get_filename_component(_stub_parent "${stub}" DIRECTORY)
    file(MAKE_DIRECTORY "${_stub_parent}")
    configure_file("${src}" "${stub}" COPYONLY)
    set(src "${stub}")

    set(singleton OFF)
    if(rel MATCHES "\\.qml$")
      file(READ "${src}" qml_text)
      if(qml_text MATCHES "(^|\n)pragma Singleton")
        set(singleton ON)
        if(rel MATCHES "^qml/[^/]+\\.qml$")
          get_filename_component(stem "${rel}" NAME_WE)
          list(APPEND singleton_lines "singleton ${stem} 1.0 ${stem}.qml")
        endif()
      endif()
    endif()
    if(singleton)
      set_source_files_properties("${src}" PROPERTIES
        QT_RESOURCE_ALIAS "${rel}"
        QT_QML_SINGLETON_TYPE TRUE)
    else()
      set_source_files_properties("${src}" PROPERTIES QT_RESOURCE_ALIAS "${rel}")
    endif()
    if(rel MATCHES "^qml/.+/.+\\.qml$")
      get_filename_component(dir "${rel}" DIRECTORY)
      get_filename_component(stem "${rel}" NAME_WE)
      if(singleton)
        list(APPEND dir_entries "${dir}|singleton ${stem} 1.0 ${stem}.qml")
      else()
        list(APPEND dir_entries "${dir}|${stem} 1.0 ${stem}.qml")
      endif()
    endif()
    list(APPEND qml_files "${src}")
  endforeach()

  # Base URL is qrc:/qt/qml/qmlcomponents/qml/. This directory qmldir keeps that
  # folder from becoming an implicit import of every sibling.
  set(inner_dir "${CMAKE_CURRENT_BINARY_DIR}/cip-qmldir/qml")
  file(MAKE_DIRECTORY "${inner_dir}")
  set(inner "${inner_dir}/qmldir")
  list(JOIN singleton_lines "\n" singleton_text)
  if(singleton_text)
    set(singleton_text "\n${singleton_text}")
  endif()
  _cip_write_if_changed("${inner}" "prefer :/qt/qml/qmlcomponents/qml/${singleton_text}\n")
  set_source_files_properties("${inner}" PROPERTIES QT_RESOURCE_ALIAS "qml/qmldir")
  list(APPEND resources "${inner}")

  set(dir_names)
  foreach(entry IN LISTS dir_entries)
    string(REGEX REPLACE "\\|.*$" "" dir "${entry}")
    list(APPEND dir_names "${dir}")
  endforeach()
  list(REMOVE_DUPLICATES dir_names)
  foreach(dir IN LISTS dir_names)
    set(lines)
    foreach(entry IN LISTS dir_entries)
      if(entry MATCHES "^${dir}\\|")
        string(REGEX REPLACE "^${dir}\\|" "" line "${entry}")
        list(APPEND lines "${line}")
      endif()
    endforeach()
    list(JOIN lines "\n" body)
    set(dir_qmldir "${CMAKE_CURRENT_BINARY_DIR}/cip-qmldir/${dir}/qmldir")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/cip-qmldir/${dir}")
    _cip_write_if_changed("${dir_qmldir}" "${body}\n")
    set_source_files_properties("${dir_qmldir}" PROPERTIES QT_RESOURCE_ALIAS "${dir}/qmldir")
    list(APPEND resources "${dir_qmldir}")
  endforeach()

  set(_cip_qml_disk_args)
  if(TIBIA_QML_DISK)
    list(APPEND _cip_qml_disk_args NO_CACHEGEN)
  endif()

  qt_add_qml_module(qmlcomponents
    URI qmlcomponents
    VERSION 1.0
    STATIC
    NO_LINT
    ${_cip_qml_disk_args}
    RESOURCE_PREFIX /qt/qml
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qml/qmlcomponents"
    QML_FILES ${qml_files}
    RESOURCES ${resources}
    IMPORTS
      QtQuick
      QtQuick.Controls
      QtQuick.Controls.Basic
      QtQuick.Layouts
      QtQuick.Window
      QtQuick.Effects
      QtQuick.Dialogs
      QtQuick.Templates
      Qt5Compat.GraphicalEffects
  )
  target_link_libraries(qmlcomponents PUBLIC Qt6::Qml Qt6::Quick)
endfunction()
