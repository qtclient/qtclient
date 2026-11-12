# Inert QtWebEngine / QtWebChannel modules: CIP's store and news pages import
# them, Qt WebEngine is not available on every platform (wasm, mobile), and
# those pages are replaced by product UI.
set_source_files_properties(QtWebEngine/WebEngineView.qml PROPERTIES QT_RESOURCE_ALIAS WebEngineView.qml)
set_source_files_properties(QtWebEngine/WebEngineProfile.qml PROPERTIES QT_RESOURCE_ALIAS WebEngineProfile.qml)
qt_add_qml_module(tibia_webengine
  URI QtWebEngine
  VERSION 1.0
  STATIC
  NO_LINT
  RESOURCE_PREFIX /qt/qml
  OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qml/QtWebEngine"
  QML_FILES
    QtWebEngine/WebEngineView.qml
    QtWebEngine/WebEngineProfile.qml
)

qt_add_qml_module(tibia_webchannel
  URI QtWebChannel
  VERSION 1.0
  STATIC
  NO_LINT
  RESOURCE_PREFIX /qt/qml
  OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qml/QtWebChannel"
  SOURCES QtWebChannel/webchannel.h
)
target_include_directories(tibia_webchannel PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/QtWebChannel")
target_link_libraries(tibia_webchannel PRIVATE Qt6::Qml)
