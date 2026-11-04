set(MAP_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/map_spectators_test.cpp
)

# Full gtest binary still expects a linkable qtclient_core (removed with tip peel).
# Compile-check field-id / storage spectator tests against tip headers.
add_library(qtclient_map_spectator_tests_objects OBJECT ${MAP_TEST_SOURCES})
set_target_properties(qtclient_map_spectator_tests_objects PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
target_include_directories(qtclient_map_spectator_tests_objects PRIVATE
    ${CMAKE_SOURCE_DIR}/client
    ${CMAKE_SOURCE_DIR}/client/tibia
    ${CMAKE_SOURCE_DIR}/client/tibia/engine
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/client/tibia/cipgen
)
# Avoid `-DCLIENT=1` — conflicts with cip_gen identifiers named CLIENT.
target_compile_definitions(qtclient_map_spectator_tests_objects PRIVATE
    FRAMEWORK_GRAPHICS
    FRAMEWORK_NET
    FRAMEWORK_XML
)
find_package(Qt6 QUIET COMPONENTS Core Gui)
if(Qt6_FOUND)
    target_link_libraries(qtclient_map_spectator_tests_objects PRIVATE Qt6::Core Qt6::Gui)
endif()
find_package(GTest CONFIG REQUIRED)
target_link_libraries(qtclient_map_spectator_tests_objects PRIVATE GTest::gtest)

add_custom_target(qtclient_map_spectator_tests DEPENDS qtclient_map_spectator_tests_objects)
