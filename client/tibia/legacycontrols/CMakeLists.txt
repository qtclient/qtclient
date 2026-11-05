# QtQuick.LegacyControls: QML from tibia-sync (install tip), C++ from the
# LGPL CipSoft-Tibia/Qt5LegacyControls submodule on branch main (not
# hash-pinned — bump when sync QML needs newer native types). Settings is a
# small stub: CIP Soft QML still references it; the public module does not
# ship the type.
set(LC_QML_ROOT "${CIP_ROOT}/client/upstream/qml/QtQuick/LegacyControls")
set(LC_CXX_ROOT "${CIP_ROOT}/third_party/Qt5LegacyControls/QtQuick/LegacyControls")
if(NOT EXISTS "${LC_QML_ROOT}/BasicTableView.qml")
  message(FATAL_ERROR
    "tibia: missing ${LC_QML_ROOT} — run make tibia-sync (carves LegacyControls "
    "QML from the installed Tibia.app plugin)")
endif()
if(NOT EXISTS "${LC_CXX_ROOT}/abstractstyle.cpp")
  message(FATAL_ERROR
    "tibia: missing ${LC_CXX_ROOT} — init submodule:\n"
    "  git submodule update --init third_party/Qt5LegacyControls")
endif()

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
  set(src "${LC_QML_ROOT}/${name}")
  if(NOT EXISTS "${src}")
    message(FATAL_ERROR "tibia: LegacyControls QML missing ${src} — re-run make tibia-sync")
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
    "${LC_CXX_ROOT}/abstractstyle.cpp"
    "${LC_CXX_ROOT}/abstractstyle.h"
    "${LC_CXX_ROOT}/calendarmodel.cpp"
    "${LC_CXX_ROOT}/calendarmodel.h"
    "${LC_CXX_ROOT}/enumerations.h"
    "${LC_CXX_ROOT}/padding.h"
    "${LC_CXX_ROOT}/rangeddate.cpp"
    "${LC_CXX_ROOT}/rangeddate.h"
    "${LC_CXX_ROOT}/rangemodel.cpp"
    "${LC_CXX_ROOT}/rangemodel.h"
    "${LC_CXX_ROOT}/rangemodelprivate.h"
    "${LC_CXX_ROOT}/wheelarea.cpp"
    "${LC_CXX_ROOT}/wheelarea.h"
    settings.cpp
    settings.h
  IMPORTS QtQml QtQuick QtQuick.Layouts QtQuick.Window
)
target_include_directories(tibia_legacycontrols PRIVATE "${LC_CXX_ROOT}")
target_link_libraries(tibia_legacycontrols PRIVATE Qt6::Quick Qt6::Qml)
set_target_properties(tibia_legacycontrols PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON)
target_compile_options(tibia_legacycontrols PRIVATE
  $<IF:$<STREQUAL:$<CXX_COMPILER_ID>,MSVC>,-W0,-w>)
