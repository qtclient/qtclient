# GeneratePackQmldir.cmake — write qml/<module>/qmldir from *.qml / *.js
# Usage: cmake -DMODULE_DIR=… -DMODULE_NAME=… -P GeneratePackQmldir.cmake

if(NOT MODULE_DIR OR NOT MODULE_NAME)
  message(FATAL_ERROR "GeneratePackQmldir: MODULE_DIR and MODULE_NAME required")
endif()

set(_lines "module ${MODULE_NAME}\n")
file(GLOB _qml_top LIST_DIRECTORIES false "${MODULE_DIR}/*.qml")
list(SORT _qml_top)
foreach(_f IN LISTS _qml_top)
  get_filename_component(_stem "${_f}" NAME_WE)
  get_filename_component(_name "${_f}" NAME)
  string(APPEND _lines "${_stem} 1.0 ${_name}\n")
endforeach()

file(GLOB _qml_comp LIST_DIRECTORIES false "${MODULE_DIR}/components/*.qml")
list(SORT _qml_comp)
foreach(_f IN LISTS _qml_comp)
  get_filename_component(_stem "${_f}" NAME_WE)
  get_filename_component(_name "${_f}" NAME)
  string(APPEND _lines "${_stem} 1.0 components/${_name}\n")
endforeach()

file(GLOB _js LIST_DIRECTORIES false "${MODULE_DIR}/*.js")
list(SORT _js)
foreach(_f IN LISTS _js)
  get_filename_component(_stem "${_f}" NAME_WE)
  get_filename_component(_name "${_f}" NAME)
  string(APPEND _lines "${_stem} 1.0 ${_name}\n")
endforeach()

file(WRITE "${MODULE_DIR}/qmldir" "${_lines}")
message(STATUS "GeneratePackQmldir: ${MODULE_DIR}/qmldir (${MODULE_NAME})")
