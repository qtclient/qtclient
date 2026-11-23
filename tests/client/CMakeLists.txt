# Ported CipSoft classes that only need QtCore: built from their own sources.
find_package(Qt6 REQUIRED COMPONENTS Core Gui)
include(GoogleTest)
function(qtclient_add_port_test TARGET_NAME)
    add_executable(${TARGET_NAME} ${ARGN})
    set_target_properties(${TARGET_NAME} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/client/tibia
        ${CMAKE_SOURCE_DIR}/client
        ${CMAKE_SOURCE_DIR}/shared/src
        # Generated CipSoft stubs (<cip_gen.h>).
        ${CMAKE_BINARY_DIR}/client/tibia/cipgen
        ${CMAKE_BINARY_DIR}/client/tibia/protobuf
    )
    target_link_libraries(${TARGET_NAME} PRIVATE GTest::gtest GTest::gtest_main Qt6::Core)
    if(TARGET cip_codegen_tibia)
        add_dependencies(${TARGET_NAME} cip_codegen_tibia)
    endif()
    gtest_discover_tests(${TARGET_NAME})
endfunction()

# Height stamps live in tip idleot/ (geometry-only; no pack required).
qtclient_add_port_test(client_heightstamp_tests
    heightstamp_test.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/idleot/heightstamp.cpp
)
target_link_libraries(client_heightstamp_tests PRIVATE Qt6::Gui)

# Weather state/sounds live in the product pack — build when the pack is linked or QTCLIENT_PACK is set.
set(_pack_client "")
if(QTCLIENT_LINKED_PACK AND EXISTS "${QTCLIENT_LINKED_PACK}/idleot/weather/weatherstate.h")
    set(_pack_client "${QTCLIENT_LINKED_PACK}")
elseif(DEFINED ENV{QTCLIENT_PACK})
    set(_pack_env "$ENV{QTCLIENT_PACK}")
    if(NOT IS_ABSOLUTE "${_pack_env}")
        # Umbrella .env uses repo-relative paths (e.g. apps/idleot-client).
        get_filename_component(_pack_env "${CMAKE_SOURCE_DIR}/../../${_pack_env}" ABSOLUTE)
    endif()
    if(EXISTS "${_pack_env}/client/idleot/weather/weatherstate.h")
        set(_pack_client "${_pack_env}/client")
    endif()
endif()
if(_pack_client)
    qtclient_add_port_test(client_weather_tests weather_test.cpp)
    target_include_directories(client_weather_tests PRIVATE "${_pack_client}")
    target_link_libraries(client_weather_tests PRIVATE Qt6::Gui)
else()
    message(STATUS "qtclient tests: skip client_weather_tests (no IdleOT pack weather headers)")
endif()

qtclient_add_port_test(client_durationtween_tests durationtween_test.cpp)
qtclient_add_port_test(client_worldmaplinearmovementtween_tests
    worldmaplinearmovementtween_test.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/animation/worldmaplinearmovementtween.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/worldmap/coordinates.cpp
)
# The meta infos a game session is built from.
qtclient_add_port_test(client_gamesessionmetainfo_tests
    gamesessionmetainfo_test.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/game/gamesessionmetainfo.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/game/gameservergamesessionmetainfo.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/game/sessiondumpgamesessionmetainfo.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/game/tutorialsessiondumpgamesessionmetainfo.cpp
)
# Classic positions of game action locations (depot search items included).
qtclient_add_port_test(client_gameactioncontext_tests
    gameactioncontext_test.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/input/gameactioncontext.cpp
)
qtclient_add_port_test(client_creaturemovement_tests
    creaturemovement_test.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/creatures/creaturemovementspeedcalculator.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/creatures/creaturemovementtween.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/animation/movingoutfitanimator.cpp
)
# CIP TAuthenticationProcessController: the login state machine against a fake login connection.
find_package(Qt6 REQUIRED COMPONENTS Network Test Gui Qml Quick)
qtclient_add_port_test(client_authenticationprocesscontroller_tests
    authenticationprocesscontroller_test.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/authentication/authenticationprocesscontroller.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/authentication/authenticationprocesscontroller.h
    ${CMAKE_SOURCE_DIR}/client/tibia/authentication/emailcoderequestuploader.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/authentication/emailcoderequestuploader.h
    ${CMAKE_SOURCE_DIR}/client/tibia/authentication/loginconfirmationcoderequestuploader.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/authentication/loginconfirmationcoderequestuploader.h
)
set_target_properties(client_authenticationprocesscontroller_tests PROPERTIES AUTOMOC ON)
target_link_libraries(client_authenticationprocesscontroller_tests PRIVATE Qt6::Network Qt6::Test Qt6::Gui Qt6::Qml Qt6::Quick qtclient_shared)
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
    # gameaction.h includes the generated cip_gen.h (tibia's cipgen include dir).
    add_dependencies(client_gameactionmapper_tests cip_codegen_tibia)
    include(GoogleTest)
    gtest_discover_tests(client_gameactionmapper_tests)

    find_package(Qt6 REQUIRED COMPONENTS Qml Quick Test)
    # CIP input::TKeyboardStatusStorage; its generated base classes come from cip_gen_05 (moc'd here).
    set(_cipgen05 ${CMAKE_BINARY_DIR}/client/tibia/cipgen/cip_gen_05.cpp)
    set_source_files_properties(${_cipgen05} PROPERTIES GENERATED TRUE)
    add_executable(client_keyboardstatusstorage_tests
        keyboardstatusstorage_test.cpp
        ${CMAKE_SOURCE_DIR}/client/tibia/input/keyboardstatusstorage.cpp
        ${CMAKE_SOURCE_DIR}/client/tibia/input/eventfilters.cpp
        ${_cipgen05}
    )
    set_target_properties(client_keyboardstatusstorage_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON AUTOMOC ON)
    target_include_directories(client_keyboardstatusstorage_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/client/tibia
        ${CMAKE_SOURCE_DIR}/client
        ${CMAKE_SOURCE_DIR}/include
        $<TARGET_PROPERTY:tibia,INCLUDE_DIRECTORIES>
    )
    target_compile_definitions(client_keyboardstatusstorage_tests PRIVATE $<TARGET_PROPERTY:tibia,COMPILE_DEFINITIONS>)
    target_link_libraries(client_keyboardstatusstorage_tests PRIVATE GTest::gtest GTest::gtest_main Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test)
    add_dependencies(client_keyboardstatusstorage_tests cip_codegen_tibia)
    gtest_discover_tests(client_keyboardstatusstorage_tests)
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
    # The processor's generated CipSoft base class (moc'd here). cip_gen_08 refers
    # to bases of the other generated units, so the test takes all of them.
    set(_cipgenall)
    foreach(_i 00 01 02 03 04 05 06 07 08 09 10 11)
        list(APPEND _cipgenall ${CMAKE_BINARY_DIR}/client/tibia/cipgen/cip_gen_${_i}.cpp)
    endforeach()
    set_source_files_properties(${_cipgenall} PROPERTIES GENERATED TRUE)
    add_executable(client_sessiondumphints_tests
        sessiondumphints_test.cpp
        ${_cipgenall}
        sessiondumphintcondition_test.cpp
        ${CMAKE_SOURCE_DIR}/shared/src/jsonvalidator.cpp
        ${CMAKE_SOURCE_DIR}/client/tibia/input/gameactioncontext.cpp
        ${CMAKE_SOURCE_DIR}/client/tibia/tutorial/sessiondumphintbase.cpp
        ${CMAKE_SOURCE_DIR}/client/tibia/tutorial/sessiondumphintpublishgameaction.cpp
        ${CMAKE_SOURCE_DIR}/client/tibia/tutorial/sessiondumphintcondition.cpp
        ${CMAKE_SOURCE_DIR}/client/tibia/tutorial/sessiondumphintconditionshowtutorialhint.cpp
        ${CMAKE_SOURCE_DIR}/client/tibia/tutorial/sessiondumphints.cpp
        ${CMAKE_SOURCE_DIR}/client/tibia/tutorial/sessiondumphintsprocessor.cpp
        ${CMAKE_SOURCE_DIR}/client/tibia/tutorial/sessiondumphintsprocessor.h
    )
    set_target_properties(client_sessiondumphints_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON AUTOMOC ON)
    target_include_directories(client_sessiondumphints_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/client/tibia
        ${CMAKE_SOURCE_DIR}/client
        ${CMAKE_SOURCE_DIR}/include
        $<TARGET_PROPERTY:tibia,INCLUDE_DIRECTORIES>
    )
    target_link_libraries(client_sessiondumphints_tests PRIVATE GTest::gtest GTest::gtest_main Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)
    target_compile_definitions(client_sessiondumphints_tests PRIVATE
        $<TARGET_PROPERTY:tibia,COMPILE_DEFINITIONS>
        QTCLIENT_TESTDATA_DIR="${CMAKE_SOURCE_DIR}/testdata")
    add_dependencies(client_sessiondumphints_tests cip_codegen_tibia)
    gtest_discover_tests(client_sessiondumphints_tests)
endif()

# CIP systemsurvey FPS history (counter, history, controller, JSON, uploader).
if(Qt6_FOUND AND TARGET tibia)
    find_package(Qt6 REQUIRED COMPONENTS Network)
    set(_fps ${CMAKE_SOURCE_DIR}/client/tibia/systemsurvey)
    add_executable(client_fpshistory_tests
        fpshistory_test.cpp
        ${_fps}/fpscounter.cpp ${_fps}/fpscounter.h
        ${_fps}/fpshistory.cpp ${_fps}/fpshistory.h
        ${_fps}/fpshistorycontroller.cpp ${_fps}/fpshistorycontroller.h
        ${_fps}/fpshistorytojsonconverter.cpp ${_fps}/fpshistorytojsonconverter.h
        ${_fps}/fpshistoryuploader.cpp ${_fps}/fpshistoryuploader.h)
    set_target_properties(client_fpshistory_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON AUTOMOC ON)
    target_include_directories(client_fpshistory_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/client/tibia
        ${CMAKE_SOURCE_DIR}/client
        ${CMAKE_SOURCE_DIR}/include
        $<TARGET_PROPERTY:tibia,INCLUDE_DIRECTORIES>)
    target_link_libraries(client_fpshistory_tests PRIVATE GTest::gtest GTest::gtest_main Qt6::Core Qt6::Gui Qt6::Network qtclient_shared)
    target_compile_definitions(client_fpshistory_tests PRIVATE $<TARGET_PROPERTY:tibia,COMPILE_DEFINITIONS>)
    add_dependencies(client_fpshistory_tests cip_codegen_tibia)
    gtest_discover_tests(client_fpshistory_tests)
endif()

# CIP sessiondump::TClientSessiondumpRecorder / TSessiondumpAsyncLoader / TSessiondumpPlayer.
if(Qt6_FOUND AND TARGET tibia AND TARGET qtclient_shared)
    find_package(Qt6 REQUIRED COMPONENTS Network)
    set(_sd ${CMAKE_SOURCE_DIR}/client/tibia/sessiondump)
    add_executable(client_sessiondumpplayer_tests
        sessiondumpplayer_test.cpp
        ${CMAKE_SOURCE_DIR}/client/tibia/shared/iodevicereader.cpp
        ${CMAKE_SOURCE_DIR}/client/tibia/shared/iodevicewriter.cpp
        ${_sd}/clientsessiondumprecorder.cpp ${_sd}/clientsessiondumprecorder.h
        ${_sd}/sessiondumpasyncloader.cpp ${_sd}/sessiondumpasyncloader.h
        ${_sd}/sessiondumpplayer.cpp ${_sd}/sessiondumpplayer.h
        ${_sd}/sessiondumpprogressclockservice.cpp
        ${_sd}/lightweightsessiondumpmessageprocessor.cpp
        ${_sd}/sessiondumpfactory.cpp
        ${_sd}/sessiondumpmessageprocessor.cpp
        ${_sd}/sessiondumpmessagereader.cpp
        ${_sd}/sessiondumpmetadatareader.cpp
        ${_sd}/sessiondumppacketprocessor.cpp
        ${_sd}/sessiondumppacketreader.cpp
        ${_sd}/sessiondumpprocessor.cpp
        ${_sd}/sessiondumpreader.cpp
        ${_sd}/sessiondumpreadersfactory.cpp)
    set_target_properties(client_sessiondumpplayer_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON AUTOMOC ON)
    target_include_directories(client_sessiondumpplayer_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/client/tibia
        ${CMAKE_SOURCE_DIR}/client
        ${CMAKE_SOURCE_DIR}/include
        $<TARGET_PROPERTY:tibia,INCLUDE_DIRECTORIES>)
    target_link_libraries(client_sessiondumpplayer_tests PRIVATE GTest::gtest Qt6::Core Qt6::Gui Qt6::Network qtclient_shared protobuf)
    target_compile_definitions(client_sessiondumpplayer_tests PRIVATE $<TARGET_PROPERTY:tibia,COMPILE_DEFINITIONS>)
    add_dependencies(client_sessiondumpplayer_tests cip_codegen_tibia)
    gtest_discover_tests(client_sessiondumpplayer_tests)
endif()

# CIP shared:: classes (apps/qtclient/shared) of the launcher's net slice.
if(TARGET qtclient_shared)
    add_executable(client_sharednet_tests sharednet_test.cpp)
    set_target_properties(client_sharednet_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON AUTOMOC ON)
    target_link_libraries(client_sharednet_tests PRIVATE GTest::gtest qtclient_shared)
    include(GoogleTest)
    gtest_discover_tests(client_sharednet_tests)

    add_executable(client_logging_tests logging_test.cpp)
    set_target_properties(client_logging_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
    target_link_libraries(client_logging_tests PRIVATE GTest::gtest GTest::gtest_main qtclient_shared)
    gtest_discover_tests(client_logging_tests)

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

# CIP appearance instance tracker and the appearance type -> spritesheet mapping.
if(TARGET cip_messages)
    set(_cipgen01 ${CMAKE_BINARY_DIR}/client/tibia/cipgen/cip_gen_01.cpp)
    set_source_files_properties(${_cipgen01} PROPERTIES GENERATED TRUE)
    qtclient_add_port_test(client_appearanceinstancetracker_tests
        appearanceinstancetracker_test.cpp
        ${CMAKE_SOURCE_DIR}/client/tibia/appearances/appearancetype.cpp
        ${CMAKE_SOURCE_DIR}/client/tibia/appearances/appearanceinstancetracker.cpp
        ${CMAKE_SOURCE_DIR}/client/tibia/appearances/appearanceinstancetracker.h
        ${CMAKE_SOURCE_DIR}/client/tibia/sprites/appearancetypespritesheetmapping.cpp
        ${CMAKE_SOURCE_DIR}/client/tibia/sprites/spritesheetsmetadatamanager.cpp
        ${_cipgen01}
    )
    target_link_libraries(client_appearanceinstancetracker_tests PRIVATE cip_messages Qt6::Gui)
    set_target_properties(client_appearanceinstancetracker_tests PROPERTIES AUTOMOC ON)
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
qtclient_add_port_test(client_worldmapcuboid_tests worldmapcuboid_test.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/worldmap/coordinates.cpp
)

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
        ${CMAKE_SOURCE_DIR}/client/tibia/skillwheel/gridconfiguration.cpp
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

# CIP sound players (TSoundPlayer, TSoundPlayerManager, TActiveSoundPlayer,
# TPlayableSound) over a silent audio source.
if(Qt6_FOUND AND TARGET tibia)
    set(_snd ${CMAKE_SOURCE_DIR}/client/tibia/sound)
    add_executable(client_soundplayer_tests
        soundplayer_test.cpp
        ${CMAKE_BINARY_DIR}/client/tibia/cipgen/cip_gen_07.h
        ${_snd}/audiobackend.h
        ${_snd}/soundmetadatamanager.h
        ${_snd}/sounddecompressorthreadpool.h
        ${_snd}/soundmanager.cpp ${_snd}/soundmanager.h
        ${_snd}/soundplayer.cpp ${_snd}/soundplayer.h
        ${_snd}/soundplayermanager.cpp ${_snd}/soundplayermanager.h
        ${_snd}/activesoundplayer.cpp ${_snd}/activesoundplayer.h)
    set_target_properties(client_soundplayer_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON AUTOMOC ON)
    target_include_directories(client_soundplayer_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/client/tibia
        ${CMAKE_SOURCE_DIR}/client
        ${CMAKE_SOURCE_DIR}/include
        $<TARGET_PROPERTY:tibia,INCLUDE_DIRECTORIES>)
    target_link_libraries(client_soundplayer_tests PRIVATE GTest::gtest GTest::gtest_main Qt6::Core Qt6::Gui)
    target_compile_definitions(client_soundplayer_tests PRIVATE $<TARGET_PROPERTY:tibia,COMPILE_DEFINITIONS>)
    add_dependencies(client_soundplayer_tests cip_codegen_tibia)
    gtest_discover_tests(client_soundplayer_tests)
endif()

# Wave 1 ports (CipSoft classes; see tools/re/port.py).
qtclient_add_port_test(client_dualconnectionurlinfo_tests
    dualconnectionurlinfo_test.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/network/dualconnectionurlinfo.cpp
)
qtclient_add_port_test(client_coderequestuploaders_tests
    coderequestuploaders_test.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/authentication/emailcoderequestuploader.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/authentication/emailcoderequestuploader.h
    ${CMAKE_SOURCE_DIR}/client/tibia/authentication/loginconfirmationcoderequestuploader.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/authentication/loginconfirmationcoderequestuploader.h
)
set_target_properties(client_coderequestuploaders_tests PROPERTIES AUTOMOC ON)
target_link_libraries(client_coderequestuploaders_tests PRIVATE Qt6::Network qtclient_shared)
qtclient_add_port_test(client_worldmapcuboidops_tests worldmapcuboidops_test.cpp)
qtclient_add_port_test(client_animationinfo_tests animationinfo_test.cpp)
qtclient_add_port_test(client_movementdirectionnames_tests movementdirectionnames_test.cpp)
qtclient_add_port_test(client_movementintent_tests movementintent_test.cpp)
qtclient_add_port_test(client_resourcespriteidentifier_tests
    resourcespriteidentifier_test.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/sprites/imageresourcespriteidentifier.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/sprites/staticminimapmarkersymbolresourcespriteidentifier.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/sprites/spriteidentifier.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/sprites/minimaptilespritesheetidentifier.cpp
)
target_link_libraries(client_resourcespriteidentifier_tests PRIVATE Qt6::Gui)

# Wave 2 ports.
qtclient_add_port_test(client_worldmapcuboidcontains_tests worldmapcuboidcontains_test.cpp)
qtclient_add_port_test(client_assetfilesystemhelper_tests
    assetfilesystemhelper_test.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/assets/assetfilesystemhelper.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/assets/asset.cpp
)
target_link_libraries(client_assetfilesystemhelper_tests PRIVATE qtclient_shared)

# Wave 3 ports.
qtclient_add_port_test(client_renderedvolume_tests renderedvolume_test.cpp)
qtclient_add_port_test(client_gameactioncontexthelper_tests
    gameactioncontexthelper_test.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/protocol/gameactioncontexthelper.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/protocol/protobufobjectpositionhelper.cpp
)
target_link_libraries(client_gameactioncontexthelper_tests PRIVATE protobuf)
# protocol.pb.h has fields named "slots" (same as the tibia target).
target_compile_definitions(client_gameactioncontexthelper_tests PRIVATE QT_NO_KEYWORDS)
qtclient_add_port_test(client_experiencegainchangetracker_tests experiencegainchangetracker_test.cpp)

# Wave 4 ports.
qtclient_add_port_test(client_authenticationprocesscontrollercoderequests_tests
    authenticationprocesscontrollercoderequests_test.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/authentication/authenticationprocesscontroller.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/authentication/authenticationprocesscontroller.h
    ${CMAKE_SOURCE_DIR}/client/tibia/authentication/emailcoderequestuploader.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/authentication/emailcoderequestuploader.h
    ${CMAKE_SOURCE_DIR}/client/tibia/authentication/loginconfirmationcoderequestuploader.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/authentication/loginconfirmationcoderequestuploader.h
)
set_target_properties(client_authenticationprocesscontrollercoderequests_tests PROPERTIES AUTOMOC ON)
target_link_libraries(client_authenticationprocesscontrollercoderequests_tests PRIVATE Qt6::Network Qt6::Gui Qt6::Qml Qt6::Quick qtclient_shared)
qtclient_add_port_test(client_spriteidentifierclone_tests
    spriteidentifierclone_test.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/sprites/imageresourcespriteidentifier.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/sprites/staticminimapmarkersymbolresourcespriteidentifier.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/sprites/spriteidentifier.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/sprites/minimaptilespritesheetidentifier.cpp
)
target_link_libraries(client_spriteidentifierclone_tests PRIVATE Qt6::Gui)

# Wave 5 ports.
qtclient_add_port_test(client_gridconfiguration_tests
    gridconfiguration_test.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/skillwheel/gridconfiguration.cpp
)
qtclient_add_port_test(client_worldmapcuboidprint_tests
    worldmapcuboidprint_test.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/worldmap/coordinates.cpp
)

# Wave 7 ports.
if(TARGET cip_messages)
    qtclient_add_port_test(client_traderstorage_tests
        traderstorage_test.cpp
        ${CMAKE_SOURCE_DIR}/client/tibia/game/traderstorage.cpp
        ${CMAKE_SOURCE_DIR}/client/tibia/appearances/appearancetype.cpp
        ${CMAKE_SOURCE_DIR}/client/tibia/sprites/spritesheetsmetadatamanager.cpp
    )
    target_link_libraries(client_traderstorage_tests PRIVATE cip_messages Qt6::Gui)
endif()
qtclient_add_port_test(client_gameservernetwork_tests
    gameservernetwork_test.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/network/gameservernetworkpacket.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/network/unencryptedrawmessagestream.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/network/gameservernetworkpacketreader.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/network/gameservernetworkpacketwriter.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/network/gameservernetworkpacketsequenceflowprocessor.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/network/gameservernetworkpacketsequenceflowprocessor.h
    ${CMAKE_SOURCE_DIR}/client/tibia/shared/iodevicereader.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/shared/iodevicewriter.cpp
)
set_target_properties(client_gameservernetwork_tests PROPERTIES AUTOMOC ON)
target_link_libraries(client_gameservernetwork_tests PRIVATE Qt6::Network)
qtclient_add_port_test(client_gameservernetworkrawdata_tests
    gameservernetworkrawdata_test.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/network/gameservernetworkpacket.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/network/gameservernetworkpacketrawdataprocessor.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/authentication/authenticationandencryptioninfo.cpp
)
target_link_libraries(client_gameservernetworkrawdata_tests PRIVATE qtclient_shared)
qtclient_add_port_test(client_hotkeyactionsetting_tests
    hotkeyactionsetting_test.cpp
    ${CMAKE_SOURCE_DIR}/client/tibia/config/hotkeyoptions.cpp
)
set_target_properties(client_hotkeyactionsetting_tests PROPERTIES AUTOMOC ON)
target_link_libraries(client_hotkeyactionsetting_tests PRIVATE Qt6::Gui)
