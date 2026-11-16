# CIP shared:: — classes CipSoft compiles into both the client and the launcher.
# One class per file pair under src/, namespace tibia::shared.

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

add_library(qtclient_shared STATIC
  src/clipboardhelper.cpp
  src/clipboardhelper.h
  src/clockservice.cpp
  src/clockservice.h
  src/colorhelper.cpp
  src/colorhelper.h
  src/coordinate.h
  src/crashhandler.cpp
  src/crashhandler.h
  src/crashreportsender.cpp
  src/crashreportsender.h
  src/downloadpool.cpp
  src/downloadpool.h
  src/executor.cpp
  src/executor.h
  src/extent.h
  src/highlighttimer.cpp
  src/highlighttimer.h
  src/iclockservice.h
  src/imagehistogram.cpp
  src/imagehistogram.h
  src/jsondocumenthttppostsender.cpp
  src/jsondocumenthttppostsender.h
  src/jsondocumentuploaderbase.cpp
  src/jsondocumentuploaderbase.h
  src/jsonhelper.cpp
  src/jsonhelper.h
  src/jsonvalidator.cpp
  src/jsonvalidator.h
  src/launcherutils.cpp
  src/launcherutils.h
  src/networkaccessmanagerauthenticationhandler.cpp
  src/networkaccessmanagerauthenticationhandler.h
  src/paginationproxymodel.cpp
  src/paginationproxymodel.h
  src/qnetworkaccessmanagerwrapper.cpp
  src/qnetworkaccessmanagerwrapper.h
  src/qnetworkreplywrapper.cpp
  src/qnetworkreplywrapper.h
  src/qtconnectionmanager.cpp
  src/qtconnectionmanager.h
  src/qtconnectionwrapper.cpp
  src/qtconnectionwrapper.h
  src/ringbufferstopwatch.cpp
  src/ringbufferstopwatch.h
  src/singlethreadedlowpriorityexecutor.cpp
  src/singlethreadedlowpriorityexecutor.h
  src/synchronousexecutor.cpp
  src/synchronousexecutor.h
  src/textbrowserwithnetwork.cpp
  src/textbrowserwithnetwork.h
  src/timelapseclockservice.cpp
  src/timelapseclockservice.h
)
set_target_properties(qtclient_shared PROPERTIES AUTOMOC ON CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_compile_definitions(qtclient_shared PRIVATE QT_NO_KEYWORDS)
target_include_directories(qtclient_shared PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(qtclient_shared PUBLIC Qt6::Widgets Qt6::Network)
