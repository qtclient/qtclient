qtclient_add_gtest(client_heightstamp_tests
    heightstamp_test.cpp
)

qtclient_add_gtest(client_weather_tests
    weather_test.cpp
)
target_include_directories(client_weather_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/client/tibia
)
# Header-only weather/worldgeometry helpers need Qt Core for QJsonObject / QImage types.
find_package(Qt6 QUIET COMPONENTS Core Gui)
if(Qt6_FOUND)
    target_link_libraries(client_weather_tests PRIVATE Qt6::Core Qt6::Gui)
endif()

# CIP TGameActionMapper Classic + Loot: Right. Compiles mapper.cpp alone; the
# test stubs filterActions so gameaction.cpp is not required.
find_package(Qt6 QUIET COMPONENTS Core Gui)
if(Qt6_FOUND AND TARGET tibia)
    add_executable(client_gameactionmapper_tests
        gameactionmapper_test.cpp
        ${CMAKE_SOURCE_DIR}/client/tibia/input/gameactionmapper.cpp
    )
    set_target_properties(client_gameactionmapper_tests PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )
    target_include_directories(client_gameactionmapper_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/client/tibia
        ${CMAKE_SOURCE_DIR}/client
        ${CMAKE_SOURCE_DIR}/include
        $<TARGET_PROPERTY:tibia,INCLUDE_DIRECTORIES>
    )
    target_link_libraries(client_gameactionmapper_tests PRIVATE
        GTest::gtest
        GTest::gtest_main
        Qt6::Core
        Qt6::Gui
    )
    target_compile_definitions(client_gameactionmapper_tests PRIVATE
        $<TARGET_PROPERTY:tibia,COMPILE_DEFINITIONS>
    )
    include(GoogleTest)
    gtest_discover_tests(client_gameactionmapper_tests)
endif()
