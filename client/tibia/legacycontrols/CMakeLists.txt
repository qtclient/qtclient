# QtQuick.LegacyControls from CipSoft-Tibia/Qt5LegacyControls (third_party/),
# with client/overrides/QtQuick/LegacyControls replacing same-named QML (e.g.
# BasicTableView default-alias order for Qt 6.10). Settings is a small stub:
# CIP QML still references it; the public module does not ship the type.
set(LC_ROOT "${CIP_ROOT}/third_party/Qt5LegacyControls/QtQuick/LegacyControls")
if(NOT EXISTS "${LC_ROOT}/CMakeLists.txt")
  message(FATAL_ERROR
    "tibia: missing ${LC_ROOT} — init submodule:\n"
    "  git submodule update --init third_party/Qt5LegacyControls")
endif()

set(legacy_overrides "${CIP_ROOT}/client/overrides/QtQuick/LegacyControls")

set(_lc_qml
  BasicTableView.qml
  BasicTableViewStyle.qml
  CalendarHeaderModel.qml
  CalendarOld.qml
  CalendarStyle.qml
  calendarutils.js
  FocusFrame.qml
  FocusFrameStyle.qml
  ScrollBarOld.qml
  ScrollViewHelper.qml
  ScrollViewOld.qml
  ScrollViewStyle.qml
  Style.qml
  TableViewColumn.qml
  TableViewItemDelegateLoader.qml
  TableViewOld.qml
  TableViewSelection.qml
  TableViewStyle.qml
  TextSingleton.qml
)

set(legacy_qml_files)
foreach(name IN LISTS _lc_qml)
  set(src "${LC_ROOT}/${name}")
  if(EXISTS "${legacy_overrides}/${name}")
    set(src "${legacy_overrides}/${name}")
  endif()
  set_source_files_properties("${src}" PROPERTIES QT_RESOURCE_ALIAS "${name}")
  if(name STREQUAL "TextSingleton.qml")
    set_source_files_properties("${src}" PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
  endif()
  list(APPEND legacy_qml_files "${src}")
endforeach()

qt_add_qml_module(tibia_legacycontrols
  URI QtQuick.LegacyControls
  VERSION 1.0
  STATIC
  NO_LINT
  RESOURCE_PREFIX /qt/qml
  OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qml/QtQuick/LegacyControls"
  QML_FILES ${legacy_qml_files}
  SOURCES
    "${LC_ROOT}/abstractstyle.cpp"
    "${LC_ROOT}/abstractstyle.h"
    "${LC_ROOT}/calendarmodel.cpp"
    "${LC_ROOT}/calendarmodel.h"
    "${LC_ROOT}/enumerations.h"
    "${LC_ROOT}/padding.h"
    "${LC_ROOT}/rangeddate.cpp"
    "${LC_ROOT}/rangeddate.h"
    "${LC_ROOT}/rangemodel.cpp"
    "${LC_ROOT}/rangemodel.h"
    "${LC_ROOT}/rangemodelprivate.h"
    "${LC_ROOT}/wheelarea.cpp"
    "${LC_ROOT}/wheelarea.h"
    settings.cpp
    settings.h
  IMPORTS QtQml QtQuick QtQuick.Layouts QtQuick.Window
)
target_include_directories(tibia_legacycontrols PRIVATE "${LC_ROOT}")
target_link_libraries(tibia_legacycontrols PRIVATE Qt6::Quick Qt6::Qml)
set_target_properties(tibia_legacycontrols PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON)
target_compile_options(tibia_legacycontrols PRIVATE
  $<IF:$<STREQUAL:$<CXX_COMPILER_ID>,MSVC>,-W0,-w>)
