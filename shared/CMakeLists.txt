# CIP shared:: — cross-cutting types used by client and launcher.
# FS home mirrors the recovered namespace (contract classes/shared.json).

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

add_library(qtclient_shared STATIC
  src/textbrowserwithnetwork.cpp
  src/textbrowserwithnetwork.h
)
set_target_properties(qtclient_shared PROPERTIES AUTOMOC ON)
target_compile_definitions(qtclient_shared PRIVATE QT_NO_KEYWORDS)
target_include_directories(qtclient_shared PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(qtclient_shared PUBLIC Qt6::Widgets Qt6::Network)
