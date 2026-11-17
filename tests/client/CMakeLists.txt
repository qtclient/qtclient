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

# Ported CipSoft classes that only need QtCore: built from their own sources.
find_package(Qt6 REQUIRED COMPONENTS Core)
include(GoogleTest)
function(qtclient_add_port_test TARGET_NAME)
    add_executable(${TARGET_NAME} ${ARGN})
    set_target_properties(${TARGET_NAME} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
    target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/client/tibia ${CMAKE_SOURCE_DIR}/client ${CMAKE_SOURCE_DIR}/shared/src)
    target_link_libraries(${TARGET_NAME} PRIVATE GTest::gtest GTest::gtest_main Qt6::Core)
    gtest_discover_tests(${TARGET_NAME})
endfunction()

qtclient_add_port_test(client_durationtween_tests durationtween_test.cpp)
qtclient_add_port_test(client_creaturemovement_tests
    creaturemovement_test.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/creatures/creaturemovementspeedcalculator.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/creatures/creaturemovementtween.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/animation/movingoutfitanimator.cpp
)
# CIP TAuthenticationProcessController: the login state machine against a fake login connection.
find_package(Qt6 REQUIRED COMPONENTS Network Test)
qtclient_add_port_test(client_authenticationprocesscontroller_tests
    authenticationprocesscontroller_test.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/authentication/authenticationprocesscontroller.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/authentication/authenticationprocesscontroller.h
)
set_target_properties(client_authenticationprocesscontroller_tests PROPERTIES AUTOMOC ON)
target_link_libraries(client_authenticationprocesscontroller_tests PRIVATE Qt6::Network Qt6::Test)
qtclient_add_port_test(client_experiencecalculator_tests
    experiencecalculator_test.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/game/experiencecalculator.cpp
)
qtclient_add_port_test(client_phaseanimator_tests
    phaseanimator_test.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/animation/phaseanimator.cpp
)
qtclient_add_port_test(client_movementpathplanner_tests
    movementpathplanner_test.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/input/movementpathplanner.cpp
)

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

# CIP TClientOptions' clientOptionsVersion chain; THotkeyOptions' default
# sets are stubbed in the test.
find_package(Qt6 REQUIRED COMPONENTS Gui)
qtclient_add_port_test(client_clientoptionsmigration_tests
    clientoptionsmigration_test.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/config/clientoptionsmigration.cpp
)
target_link_libraries(client_clientoptionsmigration_tests PRIVATE Qt6::Gui)

# CIP TAuthenticationJsonDocumentHelper: the login service's request and answers.
qtclient_add_port_test(client_authenticationjsondocumenthelper_tests
    authenticationjsondocumenthelper_test.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/authentication/authenticationjsondocumenthelper.cpp
)

qtclient_add_port_test(client_hotkeyoptions_tests
    hotkeyoptions_test.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/config/hotkeyoptions.cpp
)
set_target_properties(client_hotkeyoptions_tests PROPERTIES AUTOMOC ON)
target_link_libraries(client_hotkeyoptions_tests PRIVATE Qt6::Gui)

# CIP shared::TIODeviceReader / TIODeviceWriter (QAbstractSocket check needs Network).
find_package(Qt6 REQUIRED COMPONENTS Network)
qtclient_add_port_test(client_iodevice_tests
    iodevice_test.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/shared/iodevicereader.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/shared/iodevicewriter.cpp
)
target_link_libraries(client_iodevice_tests PRIVATE Qt6::Network)
qtclient_add_port_test(client_xteahelper_tests
    xteahelper_test.cpp
    ${CMAKE_SOURCE_DIR}/shared/src/xteahelper.cpp
)
target_include_directories(client_xteahelper_tests PRIVATE ${CMAKE_SOURCE_DIR}/shared/src)
# CIP shared:: I/O classes (qtclient_shared: compression, blobs, hashing, RSA).
qtclient_add_port_test(shared_io_tests sharedio_test.cpp)
target_link_libraries(shared_io_tests PRIVATE qtclient_shared)
qtclient_add_port_test(client_sortedvectorlistmodel_tests sortedvectorlistmodel_test.cpp)
qtclient_add_port_test(client_simplevectorabstractlistmodel_tests
    simplevectorabstractlistmodel_test.cpp
)
qtclient_add_port_test(client_coordinate_tests
    coordinate_test.cpp
)
qtclient_add_port_test(client_sessiondumpreader_tests
    sessiondumpreader_test.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/shared/iodevicereader.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/sessiondump/lightweightsessiondumpmessageprocessor.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/sessiondump/sessiondumpfactory.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/sessiondump/sessiondumpmessageprocessor.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/sessiondump/sessiondumpmessagereader.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/sessiondump/sessiondumpmetadatareader.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/sessiondump/sessiondumppacketprocessor.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/sessiondump/sessiondumppacketreader.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/sessiondump/sessiondumpprocessor.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/sessiondump/sessiondumpreader.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/sessiondump/sessiondumpreadersfactory.cpp
)
target_link_libraries(client_sessiondumpreader_tests PRIVATE Qt6::Network)
target_compile_definitions(client_sessiondumpreader_tests PRIVATE QTCLIENT_TESTDATA_DIR="${CMAKE_SOURCE_DIR}/testdata")
qtclient_add_port_test(client_timelapseclockservice_tests
    timelapseclockservice_test.cpp
    ${CMAKE_SOURCE_DIR}/shared/src/timelapseclockservice.cpp
    ${CMAKE_SOURCE_DIR}/shared/src/clockservice.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/sessiondump/sessiondumpprogressclockservice.cpp
)
qtclient_add_port_test(client_jsonvalidator_tests
    jsonvalidator_test.cpp
    ${CMAKE_SOURCE_DIR}/shared/src/jsonvalidator.cpp
)

# CIP tutorial::TSessiondumpHints and its publish-game-action hints (game actions need the generated headers).
if(Qt6_FOUND AND TARGET tibia)
    add_executable(client_sessiondumphints_tests
        sessiondumphints_test.cpp
        ${CMAKE_SOURCE_DIR}/shared/src/jsonvalidator.cpp
        ${CMAKE_SOURCE_DIR}/client/tibia/tutorial/sessiondumphintbase.cpp
        ${CMAKE_SOURCE_DIR}/client/tibia/tutorial/sessiondumphintpublishgameaction.cpp
        ${CMAKE_SOURCE_DIR}/client/tibia/tutorial/sessiondumphints.cpp
    )
    set_target_properties(client_sessiondumphints_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
    target_include_directories(client_sessiondumphints_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/client/tibia
        ${CMAKE_SOURCE_DIR}/client
        ${CMAKE_SOURCE_DIR}/include
        $<TARGET_PROPERTY:tibia,INCLUDE_DIRECTORIES>
    )
    target_link_libraries(client_sessiondumphints_tests PRIVATE GTest::gtest GTest::gtest_main Qt6::Core Qt6::Gui)
    target_compile_definitions(client_sessiondumphints_tests PRIVATE
        $<TARGET_PROPERTY:tibia,COMPILE_DEFINITIONS>
        QTCLIENT_TESTDATA_DIR="${CMAKE_SOURCE_DIR}/testdata")
    gtest_discover_tests(client_sessiondumphints_tests)
endif()

# CIP shared:: classes (apps/qtclient/shared) of the launcher's net slice.
if(TARGET qtclient_shared)
    add_executable(client_sharednet_tests sharednet_test.cpp)
    set_target_properties(client_sharednet_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON AUTOMOC ON)
    target_link_libraries(client_sharednet_tests PRIVATE GTest::gtest qtclient_shared)
    include(GoogleTest)
    gtest_discover_tests(client_sharednet_tests)

    add_executable(client_newsstorage_tests newsstorage_test.cpp ${CMAKE_SOURCE_DIR}/client/tibia/news/newsstorage.cpp)
    set_target_properties(client_newsstorage_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON AUTOMOC ON)
    target_include_directories(client_newsstorage_tests PRIVATE ${CMAKE_SOURCE_DIR}/client/tibia ${CMAKE_SOURCE_DIR}/shared/src)
    target_link_libraries(client_newsstorage_tests PRIVATE GTest::gtest GTest::gtest_main qtclient_shared)
    gtest_discover_tests(client_newsstorage_tests)

    add_executable(client_generalplayerstatisticsprovider_tests generalplayerstatisticsprovider_test.cpp
        ${CMAKE_SOURCE_DIR}/client/tibia/cyclopedia/generalplayerstatisticsprovider.cpp)
    set_target_properties(client_generalplayerstatisticsprovider_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
    target_include_directories(client_generalplayerstatisticsprovider_tests PRIVATE ${CMAKE_SOURCE_DIR}/client/tibia)
    target_link_libraries(client_generalplayerstatisticsprovider_tests PRIVATE GTest::gtest GTest::gtest_main protobuf)
    gtest_discover_tests(client_generalplayerstatisticsprovider_tests)
endif()

# CIP worldmap::TLightMap: the frame's light grid.
qtclient_add_port_test(client_lightmap_tests
    lightmap_test.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/worldmap/lightmap.cpp
)
target_link_libraries(client_lightmap_tests PRIVATE Qt6::Gui)

qtclient_add_port_test(client_specialappearances_tests
    specialappearances_test.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/assets/specialappearances.cpp
)

# CIP appearances::TAppearanceType: appearances.dat objects converted from their protobuf messages.
if(TARGET cip_messages)
    qtclient_add_port_test(client_appearancetype_tests
        appearancetype_test.cpp
        ${CMAKE_SOURCE_DIR}/client/tibia/appearances/appearancetype.cpp
        ${CMAKE_SOURCE_DIR}/client/tibia/sprites/spritesheetsmetadatamanager.cpp
    )
    target_link_libraries(client_appearancetype_tests PRIVATE cip_messages Qt6::Gui)
endif()

# CIP minimap marker file (minimapmarkers.bin) conversions.
if(TARGET cip_messages)
    qtclient_add_port_test(client_minimapmarker_tests
        minimapmarker_test.cpp
        ${CMAKE_SOURCE_DIR}/client/tibia/minimap/minimapmarker.cpp
    )
    target_link_libraries(client_minimapmarker_tests PRIVATE cip_messages)
endif()

# CIP sprite pipeline: spritesheet metadata, cache, manager and sprites cut from spritesheets.
qtclient_add_port_test(client_spritesheetmanager_tests
    spritesheetmanager_test.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/sprites/spritesheetsmetadatamanager.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/sprites/spritesheetcache.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/sprites/spritesheetdecompressor.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/sprites/spritesheetdecompressor.h
    ${CMAKE_SOURCE_DIR}/client/tibia/sprites/spritesheetdecompressorsynchronous.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/sprites/spritesheetdecompressorsynchronous.h
    ${CMAKE_SOURCE_DIR}/client/tibia/sprites/spritesheetmanager.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/sprites/spritesheetmanager.h
    ${CMAKE_SOURCE_DIR}/client/tibia/sprites/spritefromspritesheetinfo.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/sprites/spriteinspritesheet.cpp
)
target_link_libraries(client_spritesheetmanager_tests PRIVATE qtclient_shared Qt6::Gui)
set_target_properties(client_spritesheetmanager_tests PROPERTIES AUTOMOC ON)

# CIP worldmap coordinate helpers and cuboids.
qtclient_add_port_test(client_worldmapcuboid_tests worldmapcuboid_test.cpp)

# CIP cyclopedia::TCyclopediaMapStaticDataStorage: map.dat areas, files and masks.
qtclient_add_port_test(client_cyclopediamapstaticdatastorage_tests
    cyclopediamapstaticdatastorage_test.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/cyclopedia/cyclopediamapstaticdatastorage.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/cyclopedia/cyclopediamapstaticdatastorage.h
    ${CMAKE_SOURCE_DIR}/client/tibia/assets/asset.cpp
)
target_link_libraries(client_cyclopediamapstaticdatastorage_tests PRIVATE protobuf Qt6::Gui)
set_target_properties(client_cyclopediamapstaticdatastorage_tests PROPERTIES AUTOMOC ON)

# CIP skill wheel grid path solver (TSkillWheelPageController): which grids are valid.
find_package(ZLIB QUIET)
if(Qt6_FOUND AND TARGET tibia AND TARGET ZLIB::ZLIB)
    add_executable(client_skillwheeldata_tests
        skillwheeldata_test.cpp
        ${CMAKE_SOURCE_DIR}/client/tibia/skillwheel/skillwheeldata.cpp
    )
    set_target_properties(client_skillwheeldata_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
    target_include_directories(client_skillwheeldata_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/client/tibia
        ${CMAKE_SOURCE_DIR}/client
        ${CMAKE_SOURCE_DIR}/include
        $<TARGET_PROPERTY:tibia,INCLUDE_DIRECTORIES>
    )
    target_compile_definitions(client_skillwheeldata_tests PRIVATE $<TARGET_PROPERTY:tibia,COMPILE_DEFINITIONS>)
    target_link_libraries(client_skillwheeldata_tests PRIVATE GTest::gtest GTest::gtest_main Qt6::Core ZLIB::ZLIB)
    add_dependencies(client_skillwheeldata_tests cip_codegen_tibia)
    gtest_discover_tests(client_skillwheeldata_tests)
endif()
