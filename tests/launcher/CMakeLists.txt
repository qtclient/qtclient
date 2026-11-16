# Launcher classes (launcher/src) that are self-contained enough to test.
set(_launcher_src ${CMAKE_SOURCE_DIR}/launcher/src)
find_package(Qt6 REQUIRED COMPONENTS Core Network)

add_executable(launcher_package_tests
    package_test.cpp
    ${_launcher_src}/package.cpp ${_launcher_src}/package.h
    ${_launcher_src}/cloudflarecdntrace.cpp ${_launcher_src}/cloudflarecdntrace.h)
set_target_properties(launcher_package_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON AUTOMOC ON)
target_include_directories(launcher_package_tests PRIVATE ${_launcher_src} ${CMAKE_SOURCE_DIR}/shared/src)
target_link_libraries(launcher_package_tests PRIVATE GTest::gtest GTest::gtest_main Qt6::Core Qt6::Network qtclient_shared)
gtest_discover_tests(launcher_package_tests)

find_package(Qt6 REQUIRED COMPONENTS Gui)
add_executable(launcher_systemsurvey_tests
    systemsurvey_test.cpp
    ${_launcher_src}/systemsurvey/systemsurveycreator.cpp
    ${_launcher_src}/systemsurvey/systemsurveytojsonconverter.cpp
    ${_launcher_src}/systemsurvey/systemsurveyvalueretriever.cpp)
set_target_properties(launcher_systemsurvey_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_include_directories(launcher_systemsurvey_tests PRIVATE ${_launcher_src}/systemsurvey)
target_link_libraries(launcher_systemsurvey_tests PRIVATE GTest::gtest GTest::gtest_main Qt6::Core Qt6::Gui Qt6::Network)
if(APPLE)
  target_link_libraries(launcher_systemsurvey_tests PRIVATE "-framework ApplicationServices")
endif()
gtest_discover_tests(launcher_systemsurvey_tests)
