find_package(GTest CONFIG REQUIRED)

include(GoogleTest)

# Lightweight gtest binary (no peeled qtclient_core). Prefer this over linking the tip.
function(qtclient_add_gtest TARGET_NAME)
    add_executable(${TARGET_NAME} ${ARGN})
    set_target_properties(${TARGET_NAME} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )
    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/client/tibia
        ${CMAKE_SOURCE_DIR}/client
        ${CMAKE_SOURCE_DIR}/shared/src
    )
    target_link_libraries(${TARGET_NAME} PRIVATE GTest::gtest GTest::gtest_main)
    if(ASAN_ENABLED AND NOT MSVC)
        target_compile_options(${TARGET_NAME} PRIVATE -fsanitize=address)
        target_link_options(${TARGET_NAME} PRIVATE -fsanitize=address)
    endif()
    gtest_discover_tests(${TARGET_NAME})
endfunction()

add_subdirectory(client)
add_subdirectory(map)
add_subdirectory(launcher)
