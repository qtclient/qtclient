# GeneratePackQmldir.cmake — write qml/<module>/qmldir from *.qml / *.js
# Usage: cmake -DMODULE_DIR=… -DMODULE_NAME=… -P GeneratePackQmldir.cmake
#
# Every *.qml / *.js under the module directory is registered by file stem,
# including subdirectories (components/, mobile/, …) so packs can group files.

if(NOT MODULE_DIR OR NOT MODULE_NAME)
  message(FATAL_ERROR "GeneratePackQmldir: MODULE_DIR and MODULE_NAME required")
endif()

set(_lines "module ${MODULE_NAME}\n")

# Top level first, then subdirectories, each sorted (stable qmldir diffs).
file(GLOB _top LIST_DIRECTORIES false RELATIVE "${MODULE_DIR}" "${MODULE_DIR}/*.qml" "${MODULE_DIR}/*.js")
list(SORT _top)
file(GLOB_RECURSE _all LIST_DIRECTORIES false RELATIVE "${MODULE_DIR}" "${MODULE_DIR}/*.qml" "${MODULE_DIR}/*.js")
list(SORT _all)
set(_nested "")
foreach(_rel IN LISTS _all)
  if(_rel MATCHES "/")
    list(APPEND _nested "${_rel}")
  endif()
endforeach()

foreach(_rel IN LISTS _top _nested)
  get_filename_component(_stem "${_rel}" NAME_WE)
  string(APPEND _lines "${_stem} 1.0 ${_rel}\n")
endforeach()

file(WRITE "${MODULE_DIR}/qmldir" "${_lines}")
message(STATUS "GeneratePackQmldir: ${MODULE_DIR}/qmldir (${MODULE_NAME})")
