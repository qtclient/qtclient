# tibia: CIP client architecture. CIP's QML UI (upstream/, vendored by
# `make tibia-sync`, never edited) runs on Qt Quick with CIP's bootstrap;
# controllers implement the generated cip:: bases (tools/cip/gen_cpp.py);
# The engine (protocol, world, assets, net buffers) is compiled into this
# executable. Packs link the fat host ABI via ENABLE_EXPORTS / QtClient::Host
# (see cmake/QtClientHost.cmake). There is no separate gamecore library.
#
# Built two ways:
#   - from the qtclient host (add_subdirectory): desktop, wasm, mobile.
#   - standalone (cmake -S client/tibia): QML shell only.
cmake_minimum_required(VERSION 3.22)

if(NOT DEFINED PROJECT_NAME)
  project(tibia LANGUAGES CXX)
  set(TIBIA_WITH_GAMECORE OFF)
elseif(TIBIA_ENGINE_SOURCES)
  set(TIBIA_WITH_GAMECORE ON)
endif()
# Nested under the host project too — Qt5LegacyControls padding.h needs C++20
# (defaulted operator==); Qt's toolchain default is still C++17.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

get_filename_component(TIBIA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
# CipSoft shared:: classes (apps/qtclient/shared) are compiled once into
# qtclient_shared; every target here sees their headers.
include_directories("${TIBIA_ROOT}/shared/src")
include("${TIBIA_ROOT}/cmake/CipSoftDir.cmake")
include("${TIBIA_ROOT}/cmake/CipCodegen.cmake")
include("${TIBIA_ROOT}/cmake/CipQmlModule.cmake")
include("${TIBIA_ROOT}/cmake/TibiaMods.cmake")
include("${TIBIA_ROOT}/cmake/QtClientMod.cmake")
include("${TIBIA_ROOT}/cmake/QtClientHost.cmake")
include("${TIBIA_ROOT}/cmake/TibiaDesktopTip.cmake")

# Dev: load QML from the source tree (umbrella `make client-build`). Production
# packaging leaves this OFF so QML stays embedded + qmlcachegen'd.
option(TIBIA_QML_DISK "Load CIP/mod QML from disk with hot reload (dev)" OFF)

# Project FindOpenGL predates Qt's OpenGL lookup; Qt must use CMake's.
set(_tibia_module_path "${CMAKE_MODULE_PATH}")
list(REMOVE_ITEM CMAKE_MODULE_PATH "${TIBIA_ROOT}/cmake")
if(APPLE AND OPENGL_FRAMEWORK AND NOT IS_ABSOLUTE "${OPENGL_gl_LIBRARY}")
  set(OPENGL_gl_LIBRARY "${OPENGL_FRAMEWORK}")
  set(_tibia_unset_gl ON)
endif()
find_package(Qt6 6.10 QUIET COMPONENTS Core CorePrivate Gui Network Qml Quick QuickPrivate QuickControls2 ShaderTools Core5Compat)
if(_tibia_unset_gl)
  unset(OPENGL_gl_LIBRARY)
endif()
set(CMAKE_MODULE_PATH "${_tibia_module_path}")
if(NOT Qt6_FOUND)
  message(STATUS "tibia: Qt 6.10 not found (make qt, then pass CMAKE_PREFIX_PATH); skipping")
  return()
endif()

qt_standard_project_setup(REQUIRES 6.10)
set(CMAKE_UNITY_BUILD OFF)
set(CMAKE_AUTOMOC ON)
# Qt 6.10 headers use __yield without including arm_acle.h on Apple arm64.
if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
  add_compile_options("SHELL:-include arm_acle.h")
endif()

# ES/wasm/android use GLES3 (glutil.h). Desktop uses compile-only gl_win32.h — no glad.

get_target_property(_qt_core_type Qt6::Core TYPE)
if(_qt_core_type STREQUAL "STATIC_LIBRARY")
  set(TIBIA_QT_STATIC ON)
endif()

add_subdirectory(legacycontrols)
tibia_resolve_desktop_webengine(TIBIA_USE_WEBENGINE_STUBS)
if(TIBIA_USE_WEBENGINE_STUBS)
  add_subdirectory(qmlstubs)
endif()
# Resolve enabled packs before qmlcomponents so pack overrides/ layers apply.
tibia_resolve_enabled_mods()
cip_qml_module()

qt_add_executable(tibia
  main.cpp
  app/bootstrap.cpp app/bootstrap.h
  app/clientoptions.cpp app/clientoptions.h
  app/graphicsrestart.cpp app/graphicsrestart.h
  app/launcherenv.cpp app/launcherenv.h
  app/loginsplash.cpp app/loginsplash.h
  app/sessionbootstrap.cpp app/sessionbootstrap.h
  app/loginwaitdialogcontroller.cpp app/loginwaitdialogcontroller.h
  app/overrideinterceptor.cpp app/overrideinterceptor.h
  app/packqmlinterceptor.cpp app/packqmlinterceptor.h
  app/qmldisk.cpp app/qmldisk.h
  authentication/loginrequestuploader.cpp authentication/loginrequestuploader.h
  authentication/authenticationjsondocumenthelper.cpp authentication/authenticationjsondocumenthelper.h
  authentication/cloudflarechallengetypes.h
  authentication/cloudflarechallengecontroller.cpp authentication/cloudflarechallengecontroller.h
  authentication/authenticationprocesscontroller.cpp authentication/authenticationprocesscontroller.h
  shared/clientservicesuploader.cpp shared/clientservicesuploader.h
  game/boostedcreaturerequestuploader.cpp game/boostedcreaturerequestuploader.h
  game/liveinformationrequestuploader.cpp game/liveinformationrequestuploader.h
  game/showoffrequestuploader.cpp game/showoffrequestuploader.h
  news/eventqmlentry.cpp news/eventqmlentry.h
  news/eventsuploader.cpp news/eventsuploader.h
  news/newsrequestuploader.cpp news/newsrequestuploader.h
  news/newsstorage.cpp news/newsstorage.h
  news/newsdiskio.cpp news/newsdiskio.h
  news/newsentrymodels.cpp news/newsentrymodels.h
  controllers/characterselectioncontroller.cpp controllers/characterselectioncontroller.h
  gamewindow/characterlogindata.h
  gamewindow/characterinformationmodeldata.cpp gamewindow/characterinformationmodeldata.h
  gamewindow/gamewindowcontroller.cpp gamewindow/gamewindowcontroller.h
  gamewindow/newsdialogcontroller.cpp gamewindow/newsdialogcontroller.h
  gamewindow/calendardialogcontroller.cpp gamewindow/calendardialogcontroller.h
  gamewindow/naggingmessagedialogcontroller.cpp gamewindow/naggingmessagedialogcontroller.h
  gamewindow/mousecursorcontroller.cpp gamewindow/mousecursorcontroller.h
  gamewindow/sidebarmanager.cpp gamewindow/sidebarmanager.h
  gamewindow/sidebarwidgetcontrollerbase.cpp gamewindow/sidebarwidgetcontrollerbase.h
  imageproviders/optimizedborderimageprovider.cpp imageproviders/optimizedborderimageprovider.h
  imageproviders/imagestripimageprovider.cpp imageproviders/imagestripimageprovider.h
  qmlcomponents/outlinetextquickitem.cpp qmlcomponents/outlinetextquickitem.h
  qmlcomponents/cachedoutlinetextquickitem.cpp qmlcomponents/cachedoutlinetextquickitem.h
  qmlcomponents/outfittypes.cpp qmlcomponents/outfittypes.h
  qmlcomponents/texthelper.cpp qmlcomponents/texthelper.h
  qmlcomponents/tooltiphelper.cpp qmlcomponents/tooltiphelper.h
  qmlcomponents/tutorialhelper.cpp qmlcomponents/tutorialhelper.h
  renderer/renderdriver.cpp renderer/renderdriver.h
  cip_impl.h
)
cip_codegen(tibia)
target_include_directories(tibia PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
# WASM qtclient_deps historically omitted repo/client roots that desktop has —
# keep them on tibia so #include <tibia/…> and "launcher/…" resolve.
if(WASM OR EMSCRIPTEN)
  target_include_directories(tibia PRIVATE
    "${TIBIA_ROOT}"
    "${TIBIA_ROOT}/client")
endif()
target_compile_definitions(tibia PRIVATE QT_NO_KEYWORDS)

if(TIBIA_QML_DISK)
  if(WASM OR EMSCRIPTEN OR ANDROID OR IOS)
    message(FATAL_ERROR "TIBIA_QML_DISK is desktop-dev only")
  endif()
  message(STATUS "tibia: TIBIA_QML_DISK — QML from source tree, NO_CACHEGEN, F5 clears cache")
  # Colon-separated roots for DiskQmlInterceptor + F5 watcher.
  # Pack overrides only (host has no client/overrides/). In-tree mods/<id>/
  # plus QTCLIENT_LINKED_PACK(S) (e.g. the product pack/client).
  if(EXISTS "${TIBIA_ROOT}/client/overrides")
    message(FATAL_ERROR
      "tibia: host client/overrides/ must not exist (CipSoft parity). "
      "Put stock forks in the pack overrides/ tree.")
  endif()
  set(_qml_override_dirs "")
  set(_qml_mod_dirs "")
  set(_qml_disk_seen "")
  function(_tibia_qml_disk_append_root out_var candidate)
    if(NOT EXISTS "${candidate}")
      return()
    endif()
    get_filename_component(_abs "${candidate}" ABSOLUTE)
    get_filename_component(_real "${_abs}" REALPATH)
    if(_real IN_LIST _qml_disk_seen)
      return()
    endif()
    list(APPEND _qml_disk_seen "${_real}")
    set(_qml_disk_seen "${_qml_disk_seen}" PARENT_SCOPE)
    set(_cur "${${out_var}}")
    if(_cur STREQUAL "")
      set(${out_var} "${_abs}" PARENT_SCOPE)
    else()
      set(${out_var} "${_cur}:${_abs}" PARENT_SCOPE)
    endif()
  endfunction()
  foreach(_mod IN LISTS QTCLIENT_ENABLED_MODS)
    _tibia_qml_disk_append_root(_qml_override_dirs
      "${TIBIA_ROOT}/client/mods/${_mod}/overrides")
    _tibia_qml_disk_append_root(_qml_mod_dirs
      "${TIBIA_ROOT}/client/mods/${_mod}/qml")
  endforeach()
  tibia_linked_pack_dirs(_linked_for_disk)
  foreach(_linked IN LISTS _linked_for_disk)
    get_filename_component(_linked_abs "${_linked}" ABSOLUTE)
    _tibia_qml_disk_append_root(_qml_override_dirs "${_linked_abs}/overrides")
    _tibia_qml_disk_append_root(_qml_mod_dirs "${_linked_abs}/qml")
  endforeach()
  message(STATUS "tibia: TIBIA_QML_DISK mod roots=${_qml_mod_dirs}")
  message(STATUS "tibia: TIBIA_QML_DISK override roots=${_qml_override_dirs}")
  target_compile_definitions(tibia PRIVATE
    TIBIA_QML_DISK=1
    TIBIA_QML_CIP_DIR="${CIPSOFT_QML_DIR}"
    TIBIA_QML_MOD_DIR="${_qml_mod_dirs}"
    TIBIA_QML_OVERRIDES_DIR="${_qml_override_dirs}")
endif()
if(APPLE AND NOT EMSCRIPTEN)
  # Carbon: the Caps Lock state (GetCurrentKeyModifiers).
  target_link_libraries(tibia PRIVATE "-framework Carbon")
endif()
if(WIN32)
  # NOMINMAX: windows.h otherwise macros min/max. Full SDK include order and
  # rpcndr cleanup live in framework/win32_platform.h (via pch / compiler.h).
  target_compile_definitions(tibia PRIVATE NOMINMAX)
  target_link_libraries(tibia PRIVATE ws2_32 imagehlp)
endif()
# QuickPrivate: QQuickText, which CIP's outline text items wrap.
target_link_libraries(tibia PRIVATE
  Qt6::Core Qt6::CorePrivate Qt6::Gui Qt6::Network Qt6::Qml Qt6::Quick Qt6::QuickPrivate Qt6::QuickControls2
  tibia_legacycontrolsplugin
  qmlcomponentsplugin
  qtclient_shared)
# Browsers have no TCP: the game connections are QWebSockets on wasm.
if(EMSCRIPTEN)
  find_package(Qt6 6.10 REQUIRED COMPONENTS WebSockets)
  target_link_libraries(tibia PRIVATE Qt6::WebSockets)
  target_sources(tibia PRIVATE network/gameserverwebsocketdevice.cpp network/gameserverwebsocketdevice.h)
endif()
tibia_link_desktop_webengine(tibia ${TIBIA_USE_WEBENGINE_STUBS})
# Public HD scaler + mods registry / WorldView / ModLoader; packs via TibiaMods + QtClientMod.
target_sources(tibia PRIVATE
  sprites/hdscale.cpp sprites/hdscale.h
  sprites/spritesession.cpp sprites/spritesession.h
  mods/registry.cpp mods/registry.h
  mods/mod_api_v1.cpp
  mods/host_api_v1.cpp mods/host_api.h
  mods/pack_host_abi.cpp mods/pack_host_abi.h
  mods/mission_backend.h mods/attention_backend.h mods/instance_backend.h mods/weather_driver.h
  mods/lighting_types.h
  mods/worldview.cpp mods/worldview.h
  mods/modloader.cpp mods/modloader.h
  mods/mod_entry.h)
target_include_directories(tibia PRIVATE "${TIBIA_ROOT}/include")
# Host platform layer: every per-platform behaviour (platform/platform.h). One
# implementation per platform; shared code carries no platform #ifdefs.
target_sources(tibia PRIVATE platform/platform.h platform/capabilities.h)
if(EMSCRIPTEN)
  target_sources(tibia PRIVATE platform/platform_browser.cpp)
else()
  target_sources(tibia PRIVATE platform/platform_desktop.cpp)
  if(APPLE)
    target_sources(tibia PRIVATE platform/platform_mac.cpp)
  elseif(WIN32)
    target_sources(tibia PRIVATE platform/platform_windows.cpp)
  else()
    target_sources(tibia PRIVATE platform/platform_linux.cpp)
  endif()
endif()
# Host build id for ModLoader / mod_api_v1 — full SHA for artifact pinning.
# Prefer CI/env (SOURCE_COMMIT / GITHUB_SHA / QTCLIENT_HOST_BUILD_ID), else git HEAD.
set(_qtclient_host_sha "$ENV{QTCLIENT_HOST_BUILD_ID}")
if(NOT _qtclient_host_sha)
  set(_qtclient_host_sha "$ENV{SOURCE_COMMIT}")
endif()
if(NOT _qtclient_host_sha)
  set(_qtclient_host_sha "$ENV{GITHUB_SHA}")
endif()
if(NOT _qtclient_host_sha)
  execute_process(
    COMMAND git -C "${TIBIA_ROOT}" rev-parse HEAD
    OUTPUT_VARIABLE _qtclient_host_sha
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
endif()
if(NOT _qtclient_host_sha)
  set(_qtclient_host_sha "unknown")
endif()
# One generated TU carries the SHA so the rest of `tibia` stays ccache-hot across commits.
set(_qtclient_host_build_id_cpp "${CMAKE_CURRENT_BINARY_DIR}/generated/host_build_id.cpp")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(WRITE "${_qtclient_host_build_id_cpp}"
  "extern \"C\" const char qtclient_host_build_id[] = \"${_qtclient_host_sha}\";\n")
target_sources(tibia PRIVATE "${_qtclient_host_build_id_cpp}")
# Pack install stamps mod.json from this file (matches the binary, not a later git HEAD).
file(WRITE "${CMAKE_BINARY_DIR}/hostBuildId" "${_qtclient_host_sha}\n")
# Product identity: stock CipSoft defaults; enabled packs set these via PARENT_SCOPE
# (see client/mods/*/CMakeLists.txt). Applied once after mods so -D is not redefined.
set(TIBIA_PRODUCT_NAME "Tibia")
set(TIBIA_PRODUCT_ORG "CipSoft")
set(TIBIA_PRODUCT_URL "https://www.tibia.com/")
set(TIBIA_DEFAULT_CDN "")

tibia_add_enabled_mods()
tibia_apply_linked_pack_staging()

target_compile_definitions(tibia PRIVATE
  "TIBIA_PRODUCT_NAME=\"${TIBIA_PRODUCT_NAME}\""
  "TIBIA_PRODUCT_ORG=\"${TIBIA_PRODUCT_ORG}\""
  "TIBIA_PRODUCT_URL=\"${TIBIA_PRODUCT_URL}\""
  "TIBIA_DEFAULT_CDN=\"${TIBIA_DEFAULT_CDN}\"")

# Bundle identity: CIP-neutral; product mods override at runtime via primaryIdentity().
set(_tibia_bundle_id com.tibia.client)
set(_tibia_bundle_name Tibia)
set_target_properties(tibia PROPERTIES
  MACOSX_BUNDLE ON
  MACOSX_BUNDLE_GUI_IDENTIFIER ${_tibia_bundle_id}
  MACOSX_BUNDLE_BUNDLE_NAME ${_tibia_bundle_name}
  MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/macos/Info.plist.in"
  WIN32_EXECUTABLE ON
  QT_QML_ROOT_PATH "${CIPSOFT_QML_DIR}")
# Desktop qtclient Play binary is `client` (Contents/MacOS/client). Browser CDN expects tibia.js + tibia.wasm.
if(EMSCRIPTEN)
  set_target_properties(tibia PROPERTIES OUTPUT_NAME tibia)
else()
  set_target_properties(tibia PROPERTIES OUTPUT_NAME client)
endif()
# Match client/CMakeLists.txt TOGGLE_BIN_FOLDER so package_desktop --bin build/<preset>/bin finds it.
if(TOGGLE_BIN_FOLDER)
  set_target_properties(tibia PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
  if(APPLE)
    set_target_properties(tibia PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
  endif()
endif()

if(TIBIA_WITH_GAMECORE)
  target_sources(tibia PRIVATE
    app/qtplatformwindow.cpp app/qtplatformwindow.h
    chat/chatgameactionhandler.cpp chat/chatgameactionhandler.h
    shared/sharedciptypes.h
    shared/iodevicereader.cpp shared/iodevicereader.h
    shared/iodevicewriter.cpp shared/iodevicewriter.h
    shared/simplevectorabstractlistmodel.h
    config/clientsettings.cpp config/clientsettings.h
    config/tibiaconfig.cpp config/tibiaconfig.h
    app/smokeactions.cpp app/smokeactions.h
    config/configciptypes.h
    sessiondump/sessiondumpasyncloader.cpp sessiondump/sessiondumpasyncloader.h
    sessiondump/sessiondumpasyncloaderprotocol.cpp
    sessiondump/sessiondumpplayer.cpp sessiondump/sessiondumpplayer.h
    sessiondump/clientsessiondumprecorder.cpp sessiondump/clientsessiondumprecorder.h
    sessiondump/sessiondumpcatalog.cpp sessiondump/sessiondumpcatalog.h
    sessiondump/sessiondumpcatalogloader.cpp sessiondump/sessiondumpcatalogloader.h
    sessiondump/lightweightsessiondumpmessageprocessor.cpp sessiondump/lightweightsessiondumpmessageprocessor.h
    sessiondump/sessiondumpfactory.cpp sessiondump/sessiondumpfactory.h
    sessiondump/sessiondumpmessageprocessor.cpp sessiondump/sessiondumpmessageprocessor.h
    sessiondump/sessiondumpmessagereader.cpp sessiondump/sessiondumpmessagereader.h
    sessiondump/sessiondumpmetadatareader.cpp sessiondump/sessiondumpmetadatareader.h
    sessiondump/sessiondumppacketprocessor.cpp sessiondump/sessiondumppacketprocessor.h
    sessiondump/sessiondumppacketreader.cpp sessiondump/sessiondumppacketreader.h
    sessiondump/sessiondumpprocessor.cpp sessiondump/sessiondumpprocessor.h
    sessiondump/sessiondumpreader.cpp sessiondump/sessiondumpreader.h
    sessiondump/sessiondumpreadersfactory.cpp sessiondump/sessiondumpreadersfactory.h
    sessiondump/sessiondumpprogressclockservice.cpp sessiondump/sessiondumpprogressclockservice.h
    network/dualconnectionurlinfo.cpp network/dualconnectionurlinfo.h
    protocol/mapwire.cpp protocol/mapwire.h
    protocol/playerwire.cpp protocol/playerwire.h
    protocol/chatwire.cpp protocol/chatwire.h
    protocol/tradewire.cpp protocol/tradewire.h
    protocol/miscwire.cpp protocol/miscwire.h
    protocol/featurewire.cpp protocol/featurewire.h
    # Header not listed: it includes protocolmessagequeue.h (Q_OBJECT); listing
    # it would AUTOMOC a second moc unit alongside protocolmessagequeue.cpp.
    protocol/protocolciptypes.cpp
    protocol/protocolmessagequeue.cpp
    protocol/protocolclassicwire.h
    protocol/objectpositionbuilder.h
    protocol/gameactioncontexthelper.cpp protocol/gameactioncontexthelper.h
    protocol/protobufobjectpositionhelper.cpp protocol/protobufobjectpositionhelper.h
    protocol/inventoryitemwire.cpp protocol/inventoryitemwire.h
    assets/asset.cpp assets/asset.h
    assets/assetcatalog.cpp assets/assetcatalog.h
    assets/contentcatalog.cpp assets/contentcatalog.h
    assets/assetfilesystemhelper.cpp assets/assetfilesystemhelper.h
    assets/assetasyncloader.cpp assets/assetasyncloader.h
    assets/assetasyncloaderworker.cpp assets/assetasyncloaderworker.h
    assets/soundcatalog.cpp assets/soundcatalog.h
    assets/assetmanager.cpp assets/assetmanager.h
    assets/assethandler.cpp assets/assethandler.h
    authentication/emailcoderequestuploader.cpp authentication/emailcoderequestuploader.h
    authentication/loginconfirmationcoderequestuploader.cpp authentication/loginconfirmationcoderequestuploader.h
    authentication/authenticationandencryptioninfo.cpp authentication/authenticationandencryptioninfo.h
    authentication/loginprotocolmessagehandler.cpp authentication/loginprotocolmessagehandler.h
    authentication/gameserverloginprocesscontroller.cpp authentication/gameserverloginprocesscontroller.h
    taskboard/taskboardciptypes.cpp taskboard/taskboardciptypes.h
    taskboard/taskboardbountytasksstorage.cpp taskboard/taskboardbountytasksstorage.h
    taskboard/taskboardcurrenciesstorage.cpp taskboard/taskboardcurrenciesstorage.h
    taskboard/taskboardshopstorage.cpp taskboard/taskboardshopstorage.h
    taskboard/taskboardweeklytasksstorage.cpp taskboard/taskboardweeklytasksstorage.h
    taskboard/monsterlistfiltermodel.cpp taskboard/monsterlistfiltermodel.h
    taskboard/monsterlistmodelsortfilterproxy.cpp taskboard/monsterlistmodelsortfilterproxy.h
    taskboard/taskboardrewardlistmodel.cpp taskboard/taskboardrewardlistmodel.h
    news/newsciptypes.cpp news/newsciptypes.h
    client/preventcloseeventfilter.cpp client/preventcloseeventfilter.h
    common/plural.cpp common/plural.h
    common/pluralexception.h
    dailyreward/dailyrewardciptypes.cpp dailyreward/dailyrewardciptypes.h
    questlog/questlogciptypes.cpp questlog/questlogciptypes.h
    questlog/questloggameactionhandler.cpp questlog/questloggameactionhandler.h
    quickloot/quicklootciptypes.cpp quickloot/quicklootciptypes.h
    skillwheel/skillwheelciptypes.cpp skillwheel/skillwheelciptypes.h
    systemsurvey/fpscounter.cpp systemsurvey/fpscounter.h
    systemsurvey/fpshistory.cpp systemsurvey/fpshistory.h
    systemsurvey/fpshistorycontroller.cpp systemsurvey/fpshistorycontroller.h
    systemsurvey/fpshistorytojsonconverter.cpp systemsurvey/fpshistorytojsonconverter.h
    systemsurvey/fpshistoryuploader.cpp systemsurvey/fpshistoryuploader.h
    systemsurvey/slidingfpscounter.cpp systemsurvey/slidingfpscounter.h
    teamfinder/teamfinderciptypes.cpp teamfinder/teamfinderciptypes.h
    teamfinder/teamsortproxymodels.cpp teamfinder/teamsortproxymodels.h
    bossbestiary/bossbestiaryciptypes.cpp bossbestiary/bossbestiaryciptypes.h
    bossbestiary/bosstiarystorage.cpp bossbestiary/bosstiarystorage.h
    bossbestiary/bosstrackerstorage.cpp bossbestiary/bosstrackerstorage.h
    highscores/highscoresciptypes.cpp highscores/highscoresciptypes.h
    imbuing/imbuingciptypes.cpp imbuing/imbuingciptypes.h
    market/marketciptypes.cpp market/marketciptypes.h
    prey/preydata.cpp prey/preydata.h
    prey/preystorage.cpp prey/preystorage.h
    prey/preyprotocolmessagehandler.cpp prey/preyprotocolmessagehandler.h
    prey/preygameactionhandler.cpp prey/preygameactionhandler.h
    reporting/reportingciptypes.cpp reporting/reportingciptypes.h
    reporting/reportingstorage.h
    screenshot/screenshotciptypes.cpp screenshot/screenshotciptypes.h
    spells/spellsciptypes.cpp spells/spellsciptypes.h
    proficiency/proficiencyciptypes.cpp proficiency/proficiencyciptypes.h
    chat/chatchannel.cpp chat/chatchannel.h
    chat/chatmodemanager.cpp chat/chatmodemanager.h
    chat/chatprotocolmessagehandler.cpp chat/chatprotocolmessagehandler.h
    chat/gamemessagehandler.cpp chat/gamemessagehandler.h
    chat/textstorage.cpp chat/textstorage.h
    chat/textoverlayrenderprovider.cpp chat/textoverlayrenderprovider.h
    shared/tibiaurltools.cpp shared/tibiaurltools.h
    gamewindow/chatcontroller.cpp gamewindow/chatcontroller.h
    gamewindow/chatchanneltabrenderinfo.cpp gamewindow/chatchanneltabrenderinfo.h
    gamewindow/chatchannelfiltercontroller.cpp gamewindow/chatchannelfiltercontroller.h
    gamewindow/capslockstate.cpp gamewindow/capslockstate.h
    creatures/attachedeffect.cpp creatures/attachedeffect.h
    creatures/attachedeffectmanager.cpp creatures/attachedeffectmanager.h
    creatures/movementtweens.cpp creatures/movementtweens.h
    creatures/creaturemovementspeedcalculator.cpp creatures/creaturemovementspeedcalculator.h
    creatures/creaturemovementtween.cpp creatures/creaturemovementtween.h
    creatures/movementdirection.h
    creatures/creaturesciptypes.cpp creatures/creaturesciptypes.h
    creatures/partygameactionprovider.cpp creatures/partygameactionprovider.h
    creatures/inspectplayergameactionprovider.cpp creatures/inspectplayergameactionprovider.h
    creatures/creaturestorage.cpp creatures/creaturestorage.h
    friends/friendsciptypes.cpp friends/friendsciptypes.h
    friends/friendlistmodels.cpp friends/friendlistmodels.h
    gamewindow/gamewindowciptypes.cpp gamewindow/gamewindowciptypes.h
    gamewindow/remainingdialogs.cpp gamewindow/remainingdialogs.h
    gamewindow/gamesessiondisconnectreactioncontroller.cpp gamewindow/gamesessiondisconnectreactioncontroller.h
    gamewindow/whiteblacklistdialogcontroller.cpp gamewindow/whiteblacklistdialogcontroller.h
    gamewindow/reportviolationcontroller.cpp gamewindow/reportviolationcontroller.h
    gamewindow/bugreportdialogcontroller.cpp gamewindow/bugreportdialogcontroller.h
    client/gameclient.cpp client/gameclient.h
    client/gameclientgamewindow.cpp
    client/magiceffectopacity.cpp client/magiceffectopacity.h
    client/gameclientbase.cpp client/gameclientbase.h
    client/gameapplication.cpp client/gameapplication.h
    client/translationloader.cpp client/translationloader.h
    qmlcomponents/qmlcomponentsciptypes.cpp qmlcomponents/qmlcomponentsciptypes.h
    qmlcomponents/lenshelpmanager.cpp qmlcomponents/lenshelpmanager.h
    qmlcomponents/lenshelpquickitem.cpp qmlcomponents/lenshelpquickitem.h
    gamewindow/lenshelpcontroller.cpp gamewindow/lenshelpcontroller.h
    qmlcomponents/soundhelper.cpp qmlcomponents/soundhelper.h
    config/sidebaroptions.cpp config/sidebaroptions.h
    container/containerciptypes.cpp container/containerciptypes.h
    container/containergameactionprovider.cpp container/containergameactionprovider.h
    container/containerslotinformation.cpp container/containerslotinformation.h
    container/objectappearanceinstanceinfostorage.cpp container/objectappearanceinstanceinfostorage.h
    container/qmlitemslotdata.cpp container/qmlitemslotdata.h
    container/containergameactionhandler.cpp container/containergameactionhandler.h
    config/specialconditionsoptions.cpp config/specialconditionsoptions.h
    config/specialconditionobject.cpp config/specialconditionobject.h
    config/optionsarchive.cpp config/optionsarchive.h
    config/clientoptiondefaults.cpp config/clientoptiondefaults.h
    config/clientoptions.cpp config/clientoptions.h
    config/clientoptionsmodeswitcher.cpp config/clientoptionsmodeswitcher.h
    config/actionbaroptions.cpp config/actionbaroptions.h
    config/characterconfigurationmanager.cpp config/characterconfigurationmanager.h config/icharacterconfiguration.h
    config/clientoptionsmigration.cpp config/clientoptionsmigration.h
    config/optionspages.cpp config/optionspages.h
    config/hotkeyoptions.cpp config/hotkeyoptions.h
    config/tibiahotkeyactionsettingqmlwrapper.cpp config/tibiahotkeyactionsettingqmlwrapper.h
    config/configbackupreader.cpp config/configbackupreader.h
    config/clientoptionsuploader.cpp config/clientoptionsuploader.h
    app/portalurls.cpp app/portalurls.h
    game/gameactionrouter.cpp game/gameactionrouter.h
    game/gameciptypes.cpp game/gameciptypes.h
    game/playerdata.cpp game/playerdata.h
    game/playerskillstats.cpp game/playerskillstats.h
    game/tgamenotify.cpp game/tgamenotify.h
    game/sessionhooks.cpp game/sessionhooks.h
    game/thingsession.cpp game/thingsession.h
    game/creaturesession.cpp game/creaturesession.h
    game/mapsession.cpp game/mapsession.h
    game/cooldownstorage.cpp game/cooldownstorage.h
    game/experiencecalculator.cpp game/experiencecalculator.h
    gamewindow/actionbarcontroller.cpp gamewindow/actionbarcontroller.h
    gamewindow/actionbarmanager.cpp gamewindow/actionbarmanager.h
    gamewindow/cooldownbarcontroller.cpp gamewindow/cooldownbarcontroller.h
    gamewindow/actionbuttoncontroller.cpp gamewindow/actionbuttoncontroller.h
    gamewindow/assignactiondialogs.cpp gamewindow/assignactiondialogs.h
    input/keyboardshortcuthandler.cpp input/keyboardshortcuthandler.h
    input/inputciptypes.h
    input/playermovementintent.h
    input/clickareaeventfilter.cpp input/clickareaeventfilter.h
    input/eventfilters.cpp input/eventfilters.h
    input/eventfiltermanager.cpp input/eventfiltermanager.h
    input/keyboardshortcuthandlereventfilter.cpp input/keyboardshortcuthandlereventfilter.h
    input/keyboardstatusstorage.cpp input/keyboardstatusstorage.h
    input/keyeventhandler.cpp input/keyeventhandler.h
    input/recordkeyboardshortcuthandler.cpp input/recordkeyboardshortcuthandler.h
    input/domainhandlers.cpp input/domainhandlers.h
    input/genericgameactionhandler.cpp input/genericgameactionhandler.h
    input/crosshairgameactionhandlers.cpp input/crosshairgameactionhandlers.h
    input/playermovementgameactionhandler.cpp input/playermovementgameactionhandler.h
    input/playermovementinputinterpreter.cpp input/playermovementinputinterpreter.h
    input/playermovementintenthandler.cpp input/playermovementintenthandler.h
    input/movementpathplanner.cpp input/movementpathplanner.h
    gamewindow/targetselectioncontroller.cpp gamewindow/targetselectioncontroller.h
    spells/spellinfostorage.cpp spells/spellinfostorage.h
    gamewindow/messagedialogcontroller.cpp gamewindow/messagedialogcontroller.h
    gamewindow/deathdialogcontroller.cpp gamewindow/deathdialogcontroller.h
    gamewindow/createcharacterdialogcontroller.cpp gamewindow/createcharacterdialogcontroller.h
    gamewindow/createaccountdialogcontroller.cpp gamewindow/createaccountdialogcontroller.h
    gamewindow/waitdialogcontroller.cpp gamewindow/waitdialogcontroller.h
    gamewindow/optionsmenudialogcontroller.cpp gamewindow/optionsmenudialogcontroller.h
    gamewindow/fpslatencyindicatorcontroller.cpp gamewindow/fpslatencyindicatorcontroller.h
    gamewindow/aboutdialogcontroller.cpp gamewindow/aboutdialogcontroller.h
    game/igamesession.cpp game/igamesession.h
    game/gamesessionmetainfo.cpp game/gamesessionmetainfo.h
    game/gameservergamesessionmetainfo.cpp game/gameservergamesessionmetainfo.h
    game/sessiondumpgamesessionmetainfo.cpp game/sessiondumpgamesessionmetainfo.h
    game/tutorialsessiondumpgamesessionmetainfo.cpp game/tutorialsessiondumpgamesessionmetainfo.h
    game/playermovementintenthandlerconfiguration.h
    game/gamesessionbase.cpp game/gamesessionbase.h
    game/selectoutfitdialogdata.cpp game/selectoutfitdialogdata.h
    game/blessingdialogdata.cpp game/blessingdialogdata.h
    game/blessingsprotocolmessagehandler.cpp game/blessingsprotocolmessagehandler.h
    game/qmlblessingdialogdata.cpp game/qmlblessingdialogdata.h
    game/qmlblessingwithcharges.cpp game/qmlblessingwithcharges.h
    game/qmlblessinghistoryentry.cpp game/qmlblessinghistoryentry.h
    game/gameeventprotocolmessagehandler.cpp game/gameeventprotocolmessagehandler.h
    game/currentgamesession.cpp game/currentgamesession.h
    game/gameservergamesession.cpp game/gameservergamesession.h
    game/sessiondumpgamesession.cpp game/sessiondumpgamesession.h
    game/sessiondumpdmpd.cpp game/sessiondumpdmpd.h
    tutorial/tutorialciptypes.cpp tutorial/tutorialciptypes.h
    tutorial/sessiondumphintbase.cpp tutorial/sessiondumphintbase.h
    tutorial/sessiondumphintpublishgameaction.cpp tutorial/sessiondumphintpublishgameaction.h
    tutorial/sessiondumphints.cpp tutorial/sessiondumphints.h
    tutorial/sessiondumphintsprocessor.cpp tutorial/sessiondumphintsprocessor.h
    tutorial/tutorialprogressuploader.cpp tutorial/tutorialprogressuploader.h
    tutorial/tutorialsessiondumpgameactionhandler.cpp tutorial/tutorialsessiondumpgameactionhandler.h
    tutorial/sessiondumphintcondition.cpp tutorial/sessiondumphintcondition.h
    tutorial/sessiondumphintconditionshowtutorialhint.cpp tutorial/sessiondumphintconditionshowtutorialhint.h
    sound/soundprotocolmessagehandler.cpp sound/soundprotocolmessagehandler.h
    sound/soundmetadatamanager.cpp sound/soundmetadatamanager.h
    sound/soundstorage.cpp sound/soundstorage.h
    sound/audiobackend.cpp sound/audiobackend.h
    sound/soundengine.cpp sound/soundengine.h
    sound/soundconfiguration.cpp sound/soundconfiguration.h
    sound/soundmanager.cpp sound/soundmanager.h
    sound/sounddecompressorthreadpool.cpp sound/sounddecompressorthreadpool.h
    sound/soundplayer.cpp sound/soundplayer.h
    sound/soundplayermanager.cpp sound/soundplayermanager.h
    sound/activesoundplayer.cpp sound/activesoundplayer.h
    sound/soundcontrollerbase.cpp sound/soundcontrollerbase.h
    sound/soundcontrollermusic.cpp sound/soundcontrollermusic.h
    sound/soundcontrollerambiencestream.cpp sound/soundcontrollerambiencestream.h
    sound/soundcontrollerambienceobjectsound.cpp sound/soundcontrollerambienceobjectsound.h
    sound/soundcontrollercombatsound.cpp sound/soundcontrollercombatsound.h
    sound/soundcontroller.cpp sound/soundcontroller.h
    sound/gamesessionsoundprovider.cpp sound/gamesessionsoundprovider.h
    sound/objectsoundcontroller.cpp sound/objectsoundcontroller.h
    sound/weatheraudio.cpp sound/weatheraudio.h
    sound/miniaudio_impl.cpp
    game/playerstates.cpp game/playerstates.h
    game/playerstatestracker.cpp game/playerstatestracker.h
    game/skillswidgetentrydata.cpp game/skillswidgetentrydata.h
    game/offlineskilltrainingdata.cpp game/offlineskilltrainingdata.h
    cyclopedia/cyclopediaciptypes.cpp cyclopedia/cyclopediaciptypes.h
    cyclopedia/staticcontentinformation.cpp cyclopedia/staticcontentinformation.h
    cyclopedia/cyclopediainterfaces.cpp cyclopedia/cyclopediainterfaces.h
    cyclopedia/creaturetrackerstorage.cpp cyclopedia/creaturetrackerstorage.h
    cyclopedia/combatstatsqmldata.cpp cyclopedia/combatstatsqmldata.h
    cyclopedia/playeroffencestatsprovider.cpp cyclopedia/playeroffencestatsprovider.h
    game/offencestatsprovider.h
    cyclopedia/playerdefencestatsprovider.cpp cyclopedia/playerdefencestatsprovider.h
    game/defencestatsprovider.h
    cyclopedia/playermiscstatsprovider.cpp cyclopedia/playermiscstatsprovider.h
    game/miscstatsprovider.h
    game/statusbarstorage.cpp game/statusbarstorage.h
    gamewindow/appearanceinstanceroledata.cpp gamewindow/appearanceinstanceroledata.h
    gamewindow/appearanceinstancelistmodel.cpp gamewindow/appearanceinstancelistmodel.h
    gamewindow/battlelistcontroller.cpp gamewindow/battlelistcontroller.h
    gamewindow/buttonbarcontroller.cpp gamewindow/buttonbarcontroller.h
    gamewindow/containercontroller.cpp gamewindow/containercontroller.h
    gamewindow/containerstoragecontroller.cpp gamewindow/containerstoragecontroller.h
    gamewindow/contextmenucontroller.cpp gamewindow/contextmenucontroller.h
    gamewindow/dialogmanager.cpp gamewindow/dialogmanager.h
    gamewindow/draganddropcontroller.cpp gamewindow/draganddropcontroller.h
    gamewindow/edittextcontroller.cpp gamewindow/edittextcontroller.h
    gamewindow/genericentertextdialogcontroller.cpp gamewindow/genericentertextdialogcontroller.h
    gamewindow/hitpointmanabarcontroller.cpp gamewindow/hitpointmanabarcontroller.h
    gamewindow/highscoresdialogcontroller.cpp gamewindow/highscoresdialogcontroller.h
    gamewindow/mapwindowcontroller.cpp gamewindow/mapwindowcontroller.h
    gamewindow/appearancetypelistmodel.cpp gamewindow/appearancetypelistmodel.h
    gamewindow/marketcontroller.cpp gamewindow/marketcontroller.h
    market/marketstorage.cpp market/marketstorage.h
    market/marketgameactionhandler.cpp market/marketgameactionhandler.h
    gamewindow/imbuingdialogcontroller.cpp gamewindow/imbuingdialogcontroller.h
    imbuing/imbuingstorage.cpp imbuing/imbuingstorage.h
    exaltation/exaltationstorage.cpp exaltation/exaltationstorage.h
    exaltation/exaltationprotocolmessagehandler.cpp exaltation/exaltationprotocolmessagehandler.h
    exaltation/exaltationgameactionhandler.cpp exaltation/exaltationgameactionhandler.h
    gamewindow/exaltationforgedialogcontroller.cpp gamewindow/exaltationforgedialogcontroller.h
    skillwheel/skillwheeldata.cpp skillwheel/skillwheeldata.h
    skillwheel/skillwheelstorage.cpp skillwheel/skillwheelstorage.h
    skillwheel/skillwheelmodels.cpp skillwheel/skillwheelmodels.h
    skillwheel/skillwheelcontrollers.cpp skillwheel/skillwheelcontrollers.h
    skillwheel/gridconfiguration.cpp skillwheel/gridconfiguration.h
    skillwheel/qmlreshapemod.cpp skillwheel/qmlreshapemod.h
    skillwheel/gematelierreshapemoddialogcontroller.cpp skillwheel/gematelierreshapemoddialogcontroller.h
    gamewindow/minimapcontroller.cpp gamewindow/minimapcontroller.h
    gamewindow/npcdialogcontroller.cpp gamewindow/npcdialogcontroller.h
    gamewindow/uifocusmanager.cpp gamewindow/uifocusmanager.h
    gamewindow/npckeywordbuttonmodel.cpp gamewindow/npckeywordbuttonmodel.h
    gamewindow/npctradecontroller.cpp gamewindow/npctradecontroller.h
    gamewindow/outfitdialogcontroller.cpp gamewindow/outfitdialogcontroller.h
    gamewindow/configurecreaturepodiumdialogcontroller.cpp gamewindow/configurecreaturepodiumdialogcontroller.h
    gamewindow/playerinventoryandstatuscontroller.cpp gamewindow/playerinventoryandstatuscontroller.h
    gamewindow/preydialogcontroller.cpp gamewindow/preydialogcontroller.h
    gamewindow/killtrackerwidgetcontroller.cpp gamewindow/killtrackerwidgetcontroller.h
    gamewindow/loottrackingwidgetcontroller.cpp gamewindow/loottrackingwidgetcontroller.h
    gamewindow/rewardwalldialogcontroller.cpp gamewindow/rewardwalldialogcontroller.h
    gamewindow/taskboarddialogcontroller.cpp gamewindow/taskboarddialogcontroller.h
    gamewindow/taskboardmodels.cpp gamewindow/taskboardmodels.h
    gamewindow/bountytaskscontroller.cpp gamewindow/bountytaskscontroller.h
    gamewindow/weeklytaskscontroller.cpp gamewindow/weeklytaskscontroller.h
    gamewindow/taskboardshopcontroller.cpp gamewindow/taskboardshopcontroller.h
    gamewindow/spelllistwidgetcontroller.cpp gamewindow/spelllistwidgetcontroller.h
    gamewindow/unjustifiedpointswidgetcontroller.cpp gamewindow/unjustifiedpointswidgetcontroller.h
    gamewindow/imbuementtrackerwidgetcontroller.cpp gamewindow/imbuementtrackerwidgetcontroller.h
    gamewindow/creaturetrackerwidgetcontroller.cpp gamewindow/creaturetrackerwidgetcontroller.h
    gamewindow/bosstrackerwidgetcontroller.cpp gamewindow/bosstrackerwidgetcontroller.h
    gamewindow/depotsearchwidgetcontroller.cpp gamewindow/depotsearchwidgetcontroller.h
    gamewindow/playerguidewidgetcontroller.cpp gamewindow/playerguidewidgetcontroller.h
    gamewindow/socialdialogcontroller.cpp gamewindow/socialdialogcontroller.h
    gamewindow/socialbuttoncontroller.cpp gamewindow/socialbuttoncontroller.h
    gamewindow/friendsfriendlistpagecontroller.cpp gamewindow/friendsfriendlistpagecontroller.h
    gamewindow/friendsinvitespagecontroller.cpp gamewindow/friendsinvitespagecontroller.h
    gamewindow/friendsaccountsearchpagecontroller.cpp gamewindow/friendsaccountsearchpagecontroller.h
    gamewindow/friendsbadgespagecontroller.cpp gamewindow/friendsbadgespagecontroller.h
    gamewindow/friendsconfigurationpagecontroller.cpp gamewindow/friendsconfigurationpagecontroller.h
    gamewindow/teamjoinpagecontroller.cpp gamewindow/teamjoinpagecontroller.h
    gamewindow/teamassemblepagecontroller.cpp gamewindow/teamassemblepagecontroller.h
    analysers/analyserstorages.cpp analysers/analyserstorages.h
    analysers/itempricestorage.cpp analysers/itempricestorage.h
    analysers/progressstorage.cpp analysers/gainwastestorage.cpp analysers/impactstorage.cpp
    analysers/analyserwidgets.cpp analysers/analyserwidgets.h
    analysers/impactwidgets.cpp analysers/impactwidgets.h
    proficiency/proficiencystorage.cpp proficiency/proficiencystorage.h
    proficiency/proficiencyperks.cpp proficiency/proficiencyperks.h
    proficiency/weaponproficiencydialogcontroller.cpp proficiency/weaponproficiencydialogcontroller.h
    proficiency/proficiencycatalogue.cpp proficiency/proficiencycatalogue.h
    proficiency/weaponproficiencyshapedialogs.cpp proficiency/weaponproficiencyshapedialogs.h
    gamewindow/cyclopediadialogcontroller.cpp gamewindow/cyclopediadialogcontroller.h
    gamewindow/characterinfodialogcontroller.cpp gamewindow/characterinfodialogcontroller.h
    gamewindow/characterinfomodels.cpp gamewindow/characterinfomodels.h
    cyclopedia/bestiarystorage.cpp cyclopedia/bestiarystorage.h
    gamewindow/monsterdialogcontroller.cpp gamewindow/monsterdialogcontroller.h
    gamewindow/monsterbonuseffectsdialogcontroller.cpp gamewindow/monsterbonuseffectsdialogcontroller.h
    gamewindow/bosstiarydialogcontroller.cpp gamewindow/bosstiarydialogcontroller.h
    gamewindow/assignablebossmodels.cpp gamewindow/assignablebossmodels.h
    cyclopedia/characterinfostorage.cpp cyclopedia/characterinfostorage.h
    cyclopedia/generalplayerstatisticsprovider.cpp cyclopedia/generalplayerstatisticsprovider.h game/generalstatsprovider.h
    cyclopedia/itemtrackingstorage.cpp cyclopedia/itemtrackingstorage.h
    cyclopedia/housesstorage.cpp cyclopedia/housesstorage.h
    gamewindow/housesinfodialogcontroller.cpp gamewindow/housesinfodialogcontroller.h
    cyclopedia/cyclopediamapdata.cpp cyclopedia/cyclopediamapdata.h
    cyclopedia/cyclopediamapgameactionprovider.cpp cyclopedia/cyclopediamapgameactionprovider.h
    cyclopedia/cyclopediamapstaticdatastorage.cpp cyclopedia/cyclopediamapstaticdatastorage.h
    cyclopedia/cyclopediamapstaticdatastoragehost.cpp
    cyclopedia/cyclopediamapcharacterconfigurationstorage.cpp cyclopedia/cyclopediamapcharacterconfigurationstorage.h
    gamewindow/cyclopediamapdialogcontroller.cpp gamewindow/cyclopediamapdialogcontroller.h
    gamewindow/magicalarchivedialogcontroller.cpp gamewindow/magicalarchivedialogcontroller.h
    qmlcomponents/worldmapcameraviewport.cpp qmlcomponents/worldmapcameraviewport.h
    qmlcomponents/worldmapqmltypes.cpp qmlcomponents/worldmapqmltypes.h
    gamewindow/iteminfodialogcontroller.cpp gamewindow/iteminfodialogcontroller.h
    gamewindow/questlogcontroller.cpp gamewindow/questlogcontroller.h
    gamewindow/questtrackerwidgetcontroller.cpp gamewindow/questtrackerwidgetcontroller.h
    gamewindow/servermodaldialogcontroller.cpp gamewindow/servermodaldialogcontroller.h
    gamewindow/playertradecontroller.cpp gamewindow/playertradecontroller.h
    gamewindow/selectobjectamountdialogcontroller.cpp gamewindow/selectobjectamountdialogcontroller.h
    gamewindow/blessingsdialogcontroller.cpp gamewindow/blessingsdialogcontroller.h
    gamewindow/tutorialhintdialogcontroller.cpp gamewindow/tutorialhintdialogcontroller.h
    gamewindow/vocationselectiondialogcontroller.cpp gamewindow/vocationselectiondialogcontroller.h
    gamewindow/editminimapmarkerdialogcontroller.cpp gamewindow/editminimapmarkerdialogcontroller.h
    gamewindow/editvipdialogcontroller.cpp gamewindow/editvipdialogcontroller.h
    gamewindow/premiumpanelcontroller.cpp gamewindow/premiumpanelcontroller.h
    creatures/vipstorage.cpp creatures/vipstorage.h
    gamewindow/vipwidgetcontroller.cpp gamewindow/vipwidgetcontroller.h
    exivaoptions/exivaoptionsstorage.cpp exivaoptions/exivaoptionsstorage.h
    exivaoptions/exivaoptionsdialogcontroller.cpp exivaoptions/exivaoptionsdialogcontroller.h
    exivaoptions/exivaoptionsgameactionhandler.cpp exivaoptions/exivaoptionsgameactionhandler.h
    exivaoptions/exivaoptionsprotocolmessagehandler.cpp exivaoptions/exivaoptionsprotocolmessagehandler.h
    gamewindow/sidebarpanelsmanager.cpp gamewindow/sidebarpanelsmanager.h
    gamewindow/sidebarwidgetsmanager.cpp gamewindow/sidebarwidgetsmanager.h
    gamewindow/skillswidgetcontroller.cpp gamewindow/skillswidgetcontroller.h
    gamewindow/stashdialogcontroller.cpp gamewindow/stashdialogcontroller.h
    gamewindow/managelootcontainerdialogcontroller.cpp gamewindow/managelootcontainerdialogcontroller.h
    quickloot/managedcontainerstorage.cpp quickloot/managedcontainerstorage.h
    quickloot/quicklootblackwhiteliststorage.cpp quickloot/quicklootblackwhiteliststorage.h
    gamewindow/statusbarcontroller.cpp gamewindow/statusbarcontroller.h
    gamewindow/storeandresourcebalancehelpercontroller.cpp gamewindow/storeandresourcebalancehelpercontroller.h
    gamewindow/storedialogcontroller.cpp gamewindow/storedialogcontroller.h
    gamewindow/bossdifficultyselectiondialogcontroller.cpp gamewindow/bossdifficultyselectiondialogcontroller.h
    gamewindow/linkcopywarningdialogcontroller.cpp gamewindow/linkcopywarningdialogcontroller.h
    gamewindow/hirelingnamechangecontroller.cpp gamewindow/hirelingnamechangecontroller.h
    gamewindow/tutorialoverlaycontroller.cpp gamewindow/tutorialoverlaycontroller.h
    gamewindow/tutorialoverlaymousetrailanimationcontroller.cpp gamewindow/tutorialoverlaymousetrailanimationcontroller.h
    gamewindow/infobannercontroller.cpp gamewindow/infobannercontroller.h
    gamewindow/viprenderinfo.cpp gamewindow/viprenderinfo.h
    gamewindow/marketcointransactiondetailsdialogcontroller.cpp gamewindow/marketcointransactiondetailsdialogcontroller.h
    gamewindow/sidebarpanelcontrollerbase.h
    gamewindow/passiveabilitylistdatamodel.cpp gamewindow/passiveabilitylistdatamodel.h
    gamewindow/passiveabilitylistdatamodelsortfilterproxy.cpp gamewindow/passiveabilitylistdatamodelsortfilterproxy.h
    gamewindow/battlelistgameactionhandler.cpp gamewindow/battlelistgameactionhandler.h
    gamewindow/taskboardhuntingtaskrewardpurchaseconfirmationdialogcontroller.cpp gamewindow/taskboardhuntingtaskrewardpurchaseconfirmationdialogcontroller.h
    gamewindow/twofactorcodeentertextdialogcontroller.cpp gamewindow/twofactorcodeentertextdialogcontroller.h
    gamewindow/tooltiptextvisitor.cpp gamewindow/tooltiptextvisitor.h
    gamewindow/inspectobjectdata.h
    gamewindow/appearancetypefilterselection.cpp gamewindow/appearancetypefilterselection.h
    input/gameaction.cpp input/gameaction.h
    input/gameactioncontext.cpp input/gameactioncontext.h
    input/gameactionhandlers.cpp input/gameactionhandlers.h
    input/gameactionmapper.cpp input/gameactionmapper.h
    input/genericobjectgameactionprovider.cpp input/genericobjectgameactionprovider.h
    input/genericchatgameactionprovider.cpp input/genericchatgameactionprovider.h
    input/actionbuttongameactionhandler.cpp input/actionbuttongameactionhandler.h
    input/myexception.cpp input/myexception.h
    input/multiactionprioritysolver.cpp input/multiactionprioritysolver.h
    input/keyboardinputtogameactionconverter.cpp input/keyboardinputtogameactionconverter.h
    minimap/minimapdiskio.cpp minimap/minimapdiskio.h
    minimap/minimapgameactionprovider.cpp minimap/minimapgameactionprovider.h
    minimap/minimapmanager.cpp minimap/minimapmanager.h
    minimap/minimapmarkerstorage.cpp minimap/minimapmarkerstorage.h
    minimap/minimapmarker.cpp minimap/minimapmarker.h
    minimap/minimaprenderprovider.cpp minimap/minimaprenderprovider.h
    minimap/minimaptile.cpp minimap/minimaptile.h
    minimap/minimaptilestorage.cpp minimap/minimaptilestorage.h
    minimap/minimapvisiblearea.cpp minimap/minimapvisiblearea.h
    minimap/minimapworldfeed.cpp minimap/minimapworldfeed.h
    minimap/waypointcosts.cpp minimap/waypointcosts.h
    minimap/minimapmarkerqmlrenderinfo.cpp minimap/minimapmarkerqmlrenderinfo.h
    minimap/minimapdiskiomemoryonly.cpp minimap/minimapdiskiomemoryonly.h
    minimap/minimaptilemanager.cpp minimap/minimaptilemanager.h
    minimap/minimapimageschache.cpp minimap/minimapimageschache.h
    minimap/minimaprenderinfostorage.cpp minimap/minimaprenderinfostorage.h
    minimap/minimapmarkercontroller.cpp minimap/minimapmarkercontroller.h
    minimap/minimapmarkeroverlaycontroller.cpp minimap/minimapmarkeroverlaycontroller.h
    minimap/minimapmarkerrenderprovider.cpp minimap/minimapmarkerrenderprovider.h
    minimap/minimaprendererscenegraphdrawcommandprovider.cpp minimap/minimaprendererscenegraphdrawcommandprovider.h
    minimap/waypointsectors.cpp minimap/waypointsectors.h
    minimap/minimapmarkergameactionhandler.cpp minimap/minimapmarkergameactionhandler.h
    minimap/minimapprotocolmessagehandler.cpp minimap/minimapprotocolmessagehandler.h
    network/extendedopcoderouter.cpp network/extendedopcoderouter.h
    protocol/objectdata.cpp protocol/objectdata.h
    protocol/protocolclientmessageprocessor.cpp protocol/protocolclientmessageprocessor.h
    protocol/unknowngameserverexception.cpp protocol/unknowngameserverexception.h
    protocol/protocolservermessageprocessor.cpp protocol/protocolservermessageprocessor.h
    protocol/protocolserverpacketprocessor.cpp protocol/protocolserverpacketprocessor.h
    protocol/protocolreader.cpp protocol/protocolreader.h
    protocol/clientwire.cpp
    protocol/protocolwriter.cpp protocol/protocolwriter.h
    protocol/protobufclientmessagetranslator.cpp protocol/protobufclientmessagetranslator.h
    network/producttransport.cpp network/producttransport.h
    network/gameserverpacketprocessor.cpp network/gameserverpacketprocessor.h
    network/gameserverdualconnection.cpp network/gameserverdualconnection.h
    network/gameservernetworkpacket.cpp network/gameservernetworkpacket.h
    network/unencryptedrawmessagestream.cpp network/unencryptedrawmessagestream.h
    network/gameservernetworkpacketreader.cpp network/gameservernetworkpacketreader.h
    network/gameservernetworkpacketwriter.cpp network/gameservernetworkpacketwriter.h
    network/gameservernetworkpacketrawdataprocessor.cpp network/gameservernetworkpacketrawdataprocessor.h
    network/gameservertcpconnection.cpp network/gameservertcpconnection.h
    network/gameservernetworkpacketconnection.cpp network/gameservernetworkpacketconnection.h
    network/dualconnectioninfo.cpp network/dualconnectioninfo.h
    network/gameservernetworkpacketsequenceflowprocessor.cpp network/gameservernetworkpacketsequenceflowprocessor.h
    network/networkqualityprotocolmessagehandler.cpp network/networkqualityprotocolmessagehandler.h
    appearances/appearanceinstance.cpp appearances/appearanceinstance.h
    appearances/objectappearanceinstancetypeid.h
    appearances/genericstorage.cpp appearances/genericstorage.h
    appearances/appearanceinstancetracker.cpp appearances/appearanceinstancetracker.h
    appearances/appearanceinstancespritesequence.cpp appearances/appearanceinstancespritesequence.h
    appearances/spriteinfo.cpp appearances/spriteinfo.h
    appearances/worldmaphighlightbaseplate.cpp appearances/worldmaphighlightbaseplate.h
    appearances/creatureplaceholderappearanceinstance.cpp appearances/creatureplaceholderappearanceinstance.h
    appearances/useableappearances.h
    appearances/outfitcolorset.cpp appearances/outfitcolorset.h
    animation/animator.h
    animation/worldmaplinearmovementtween.cpp animation/worldmaplinearmovementtween.h
    animation/animationinfo.h
    animation/phaseanimator.cpp animation/phaseanimator.h
    animation/durationtween.h
    animation/movingoutfitanimator.cpp animation/movingoutfitanimator.h
    creatures/creatureprotocolapply.cpp creatures/creatureprotocolapply.h
    creatures/creaturesgameactionhandler.cpp creatures/creaturesgameactionhandler.h
    creatures/battlelistrenderprovider.cpp creatures/battlelistrenderprovider.h
    creatures/battlelistgameactionprovider.cpp creatures/battlelistgameactionprovider.h
    effects/effectprotocolmessagehandler.cpp effects/effectprotocolmessagehandler.h
    effects/effectstorage.cpp effects/effectstorage.h
    effects/numericaleffect.cpp effects/numericaleffect.h
    effects/effectgameactionhandler.cpp effects/effectgameactionhandler.h
    effects/numericaleffectrenderprovider.cpp effects/numericaleffectrenderprovider.h
    sprites/sprite.h sprites/singlesprite.h
    sprites/spriteinspritesheet.cpp sprites/spriteinspritesheet.h
    sprites/spritefromspritesheetinfo.cpp sprites/spritefromspritesheetinfo.h
    sprites/spritesheetsmetadatamanager.cpp sprites/spritesheetsmetadatamanager.h
    sprites/appearancetypespritesheetmapping.cpp sprites/appearancetypespritesheetmapping.h
    sprites/spritesheetcache.cpp sprites/spritesheetcache.h
    sprites/spritesheetdecompressor.cpp sprites/spritesheetdecompressor.h
    sprites/spritesheetdecompressorsynchronous.cpp sprites/spritesheetdecompressorsynchronous.h
    sprites/spritesheetdecompressorthreadpool.cpp sprites/spritesheetdecompressorthreadpool.h
    sprites/spritesheetmanager.cpp sprites/spritesheetmanager.h
    sprites/spriteresolver.cpp sprites/spriteresolver.h
    sprites/resourcespriteidentifier.h
    sprites/imageresourcespriteidentifier.cpp sprites/imageresourcespriteidentifier.h
    sprites/staticminimapmarkersymbolresourcespriteidentifier.cpp sprites/staticminimapmarkersymbolresourcespriteidentifier.h
    sprites/minimaptilespritesheetidentifier.cpp sprites/minimaptilespritesheetidentifier.h
    sprites/spriteidentifier.cpp sprites/spriteidentifier.h
    sprites/spritesystem.cpp sprites/spritesystem.h
    sprites/resourcespritesmanager.cpp sprites/resourcespritesmanager.h
    sprites/qttextureatlastexturecreator.cpp sprites/qttextureatlastexturecreator.h
    sprites/spritesheetdecompressorconcurrentexception.h
    shared/tibiaclipboardhelper.cpp shared/tibiaclipboardhelper.h
    shared/experiencegainchangetracker.h
    shared/nullexperiencegainchangetracker.h
    shared/nullexperienceperhourprovider.h
    shared/dynamicresourceshelper.cpp shared/dynamicresourceshelper.h
    qmlcomponents/appearanceinstancerenderer.cpp qmlcomponents/appearanceinstancerenderer.h
    qmlcomponents/minimapquickitem.cpp qmlcomponents/minimapquickitem.h
    qmlcomponents/minimaprendererquickitem.cpp qmlcomponents/minimaprendererquickitem.h
    qmlcomponents/numericaleffectoverlay.cpp qmlcomponents/numericaleffectoverlay.h
    qmlcomponents/singleobjectrenderer.cpp qmlcomponents/singleobjectrenderer.h
    qmlcomponents/speechbubbleoverlay.cpp qmlcomponents/speechbubbleoverlay.h
    qmlcomponents/qmlincubatorwithcallbackonready.h
    qmlcomponents/cachedoutlinetexttexturemanager.cpp qmlcomponents/cachedoutlinetexttexturemanager.h
    qmlcomponents/offscreenoutlinetexttoimageservice.cpp qmlcomponents/offscreenoutlinetexttoimageservice.h
    qmlcomponents/qsgscenegraphdrawcommandsnode.cpp qmlcomponents/qsgscenegraphdrawcommandsnode.h
    qmlcomponents/appearanceinstancerendererbase.cpp qmlcomponents/appearanceinstancerendererbase.h
    qmlcomponents/simpletextureshaderscenegraphnode.cpp qmlcomponents/simpletextureshaderscenegraphnode.h
    qmlcomponents/textureatlasviewer.cpp qmlcomponents/textureatlasviewer.h
    qmlcomponents/textureatlasviewercontroller.cpp qmlcomponents/textureatlasviewercontroller.h
    renderer/childrenscenegraphnode.h
    renderer/highlightedspritenode.cpp renderer/highlightedspritenode.h
    renderer/lightmodel.cpp renderer/lightmodel.h
    renderer/worldgeometry.cpp renderer/worldgeometry.h
    renderer/weatherframe.h
    renderer/lightmappainteditem.cpp renderer/lightmappainteditem.h
    renderer/nextframeupdater.cpp renderer/nextframeupdater.h
    renderer/rendervolumeprovider.cpp renderer/rendervolumeprovider.h
    renderer/scenegraphnodeprovider.cpp renderer/scenegraphnodeprovider.h
    renderer/spritetexturecache.cpp renderer/spritetexturecache.h
    renderer/spritetextureprovider.cpp renderer/spritetextureprovider.h
    renderer/worldmapquickitem.cpp renderer/worldmapquickitem.h
    renderer/worldmaprenderprovider.cpp renderer/worldmaprenderprovider.h
    renderer/worldmapcameraviewport.cpp renderer/worldmapcameraviewport.h
    renderer/worldmapcamera.cpp renderer/worldmapcamera.h
    renderer/scenegraphdrawcommand.cpp renderer/scenegraphdrawcommand.h
    renderer/renderinfo.h
    store/storeciptypes.cpp store/storeciptypes.h
    store/dynamicallyloadedimage.cpp store/dynamicallyloadedimage.h
    store/qmlstoreobjects.cpp store/qmlstoreobjects.h
    store/storestorage.cpp store/storestorage.h
    store/storegameactionhandler.cpp store/storegameactionhandler.h
    trade/tradeciptypes.cpp trade/tradeciptypes.h
    trade/charactertrademodels.cpp trade/charactertrademodels.h
    trade/npctradestorage.cpp trade/npctradestorage.h
    trade/playertradestorage.cpp trade/playertradestorage.h
    worldmap/ambientlightstorage.cpp worldmap/ambientlightstorage.h
    worldmap/cacheforworldmapstoragelookupmembers.cpp worldmap/cacheforworldmapstoragelookupmembers.h
    worldmap/coordinates.cpp worldmap/coordinates.h
    worldmap/worldmapareadata.h
    worldmap/creaturehudqmlrenderinfo.cpp worldmap/creaturehudqmlrenderinfo.h
    worldmap/creaturehudoverlaycontroller.cpp worldmap/creaturehudoverlaycontroller.h
    worldmap/creaturehudcontroller.cpp worldmap/creaturehudcontroller.h
    worldmap/creaturehudrenderprovider.cpp worldmap/creaturehudrenderprovider.h
    worldmap/lightmap.cpp worldmap/lightmap.h
    worldmap/nextframetimestamppredictor.cpp worldmap/nextframetimestamppredictor.h
    worldmap/worldmapfield.h
    worldmap/worldmapgameactionhandler.cpp worldmap/worldmapgameactionhandler.h
    worldmap/worldmapgameactionprovider.cpp worldmap/worldmapgameactionprovider.h
    worldmap/worldmappicker.cpp worldmap/worldmappicker.h
    worldmap/worldmapprotocolmessagehandler.cpp worldmap/worldmapprotocolmessagehandler.h
    worldmap/worldmapstorage.cpp worldmap/worldmapstorage.h
    worldmap/worldmaptutorialmarkeroverlaycontroller.cpp worldmap/worldmaptutorialmarkeroverlaycontroller.h
    worldmap/worldmapobjectcounter.cpp worldmap/worldmapobjectcounter.h
    worldmap/worldmapviewport.cpp worldmap/worldmapviewport.h)
  target_sources(tibia PRIVATE ${TIBIA_ENGINE_SOURCES}
    runtime/host.cpp
    runtime/gamehost.h
    schedule/qtschedule.cpp)
  foreach(_engine_src IN LISTS TIBIA_ENGINE_SOURCES)
    if(_engine_src MATCHES "dispatch\\.gen\\.cpp$")
      set_source_files_properties("${_engine_src}" PROPERTIES SKIP_AUTOMOC ON)
    endif()
  endforeach()
  # protocolciptypes.h pulls in protocolmessagequeue.h (Q_OBJECT). Do not run a
  # second AUTOMOC pass here; protocolmessagequeue.cpp is the sole moc entry point.
  set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/protocol/protocolciptypes.cpp"
    PROPERTIES SKIP_AUTOMOC ON)
  # Engine translation units expect the old framework preamble (format, numbers,
  # asio). Force-include it here so it does not apply to Qt sources.
  set(_engine_cpp ${TIBIA_ENGINE_SOURCES})
  list(FILTER _engine_cpp INCLUDE REGEX "\\.cpp$")
  if(MSVC)
    set_property(SOURCE ${_engine_cpp} APPEND PROPERTY COMPILE_OPTIONS "/FIframework/pch.h")
  else()
    set_property(SOURCE ${_engine_cpp} APPEND PROPERTY COMPILE_OPTIONS "-include" "framework/pch.h")
  endif()
  target_link_libraries(tibia PRIVATE qtclient_deps protobuf)
  add_dependencies(tibia protocol_dispatch_check)
  # miniaudio (CIP music/ambience/UI); tip .ogg decode via libvorbis in
  # miniaudio_impl.cpp (miniaudio custom backend).
  find_package(Threads REQUIRED)
  target_link_libraries(tibia PRIVATE Threads::Threads)
  target_compile_definitions(tibia PRIVATE TIBIA_WITH_GAMECORE)
  # Ensure Phase-5 session-dump / sound protocol units are always on the link
  # line (some cmake generators dropped them from an earlier configure).
  target_sources(tibia PRIVATE
    game/sessiondumpgamesession.cpp game/sessiondumpgamesession.h
    game/sessiondumpdmpd.cpp game/sessiondumpdmpd.h
    game/passiveabilityinfostorage.cpp game/passiveabilityinfostorage.h
    game/playergameactionhandler.cpp game/playergameactionhandler.h
    game/tutorialsessiondumpgamesession.cpp game/tutorialsessiondumpgamesession.h
    game/premiummessage.cpp game/premiummessage.h
    game/premiummessagerenewpremium.cpp game/premiummessagerenewpremium.h
    game/traderstorage.cpp game/traderstorage.h
    game/fpsdropslogger.cpp game/fpsdropslogger.h
    game/gameworldvolumestorage.cpp game/gameworldvolumestorage.h
    game/configurecreaturepodiumdata.h
    tutorial/sessiondumphintsprocessor.cpp tutorial/sessiondumphintsprocessor.h
    sound/soundprotocolmessagehandler.cpp sound/soundprotocolmessagehandler.h)
endif()
if(NOT CMAKE_BUILD_TYPE STREQUAL "Release" OR NOT TIBIA_WITH_GAMECORE)
  target_compile_definitions(tibia PRIVATE TIBIA_SOURCE_DATA_DIR="${TIBIA_ROOT}/data")
endif()

if(TIBIA_QT_STATIC)
  # Static Qt (wasm): plugins are not loaded from disk. qt_import_qml_plugins on
  # the executable only sees tibia's own QML (none) — CIP UI lives in the
  # qmlcomponents static plugin — so link every Qt module CIP/mod import.
  set(_tibia_static_qml_plugins
    Qt6::qtquick2plugin
    Qt6::qmlplugin
    Qt6::modelsplugin
    Qt6::workerscriptplugin
    Qt6::qquicklayoutsplugin
    Qt6::quickwindow
    Qt6::qtquickcontrols2plugin
    Qt6::qtquickcontrols2implplugin
    Qt6::qtquicktemplates2plugin
    Qt6::qtquickcontrols2basicstyleplugin
    Qt6::qtquickcontrols2basicstyleimplplugin
    Qt6::qtquickcontrols2universalstyleplugin
    Qt6::qtquickcontrols2universalstyleimplplugin
    Qt6::qtgraphicaleffectsplugin
    Qt6::qtgraphicaleffectsprivate
    Qt6::effectsplugin
    Qt6::qtquickdialogsplugin
    Qt6::qtquickdialogs2quickimplplugin)
  foreach(_plugin IN LISTS _tibia_static_qml_plugins)
    if(TARGET ${_plugin})
      target_link_libraries(tibia PRIVATE ${_plugin})
    else()
      message(WARNING "tibia static: missing QML plugin target ${_plugin}")
    endif()
  endforeach()
  qt_import_qml_plugins(tibia)
  target_compile_definitions(tibia PRIVATE TIBIA_QT_STATIC)
endif()

# Options defaults, client strings and shaders (kind A: dump or upstream).
if(CIPSOFT_KIND_A_DIR)
  set(_cip_data_base "${CIPSOFT_KIND_A_DIR}/client")
else()
  set(_cip_data_base "${CIP_ROOT}/client/upstream")
endif()
file(GLOB cip_shaders CONFIGURE_DEPENDS "${CIPSOFT_SHADERS_DIR}/*.qsb")
# CIP's bundled client settings (:/conf/config.ini). Host: a kind-A dump older
# than the config.ini vendoring has none; the vendored copy stands in.
set(_cip_config_ini "${CIPSOFT_CONF_DIR}/config.ini")
if(NOT EXISTS "${_cip_config_ini}")
  set(_cip_config_ini "${CIP_ROOT}/client/upstream/conf/config.ini")
  set_source_files_properties("${_cip_config_ini}" PROPERTIES QT_RESOURCE_ALIAS "conf/config.ini")
endif()
qt_add_resources(tibia cip_data PREFIX / BASE "${_cip_data_base}" FILES
  "${CIPSOFT_CONF_DIR}/clientoptions.json"
  "${_cip_config_ini}"
  "${CIPSOFT_STRINGS_DIR}/client.en.qm"
  ${cip_shaders})

# Classic light-map multiply overlay (stock CIP lighting path).
if(TIBIA_WITH_GAMECORE)
  set(_stock_shader_glsl "300 es")
  if(NOT EMSCRIPTEN)
    set(_stock_shader_glsl "100 es,300 es,330,440")
  endif()
  set(_stock_shader_base "${CMAKE_CURRENT_SOURCE_DIR}/renderer/shaders")
  qt_add_shaders(tibia tibia_lightmap_shaders
    PREFIX /tibia/shaders
    BASE "${_stock_shader_base}"
    GLSL "${_stock_shader_glsl}"
    FILES
      "${_stock_shader_base}/lightmap_overlay.frag"
      "${_stock_shader_base}/lightmap_overlay.vert")
endif()

# CIP's default hotkey presets. Spell table is CIP-native on disk after
# `make extract` (`data/assets/spells.json`); do not embed pipeline game-tables.
qt_add_resources(tibia tibia_hotkeys PREFIX / BASE "${CMAKE_CURRENT_SOURCE_DIR}" FILES config/defaulthotkeys.json)


if(EMSCRIPTEN)
  # Browser qtclient is the separate Widgets `launcher` target (launcher.wasm).
  # This tip is game-only QML — never embed qtclient / Widgets here.
  # Product packs STATIC-link into this tip — no MAIN_MODULE / host_exports.

  # Embind SIOF: register primitives before other dynamic initializers
  # (see wasm_embind_bootstrap.cpp). Ignore dupes when builtin bindings re-run.
  target_sources(tibia PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/wasm_embind_bootstrap.cpp")
  target_link_options(tibia PRIVATE
    "SHELL:--post-js=${TIBIA_ROOT}/browser/embind-ignore-dupes.js")

  include("${TIBIA_ROOT}/cmake/QtclientBrowserRuntime.cmake")
  qtclient_browser_runtime(tibia)
  target_link_options(tibia PRIVATE
    "SHELL:-sOFFSCREEN_FRAMEBUFFER"
    "SHELL:-sENVIRONMENT=web,worker"
    "SHELL:-sPTHREAD_POOL_SIZE_STRICT=0"
    "SHELL:-sALLOW_MEMORY_GROWTH=0")

  # Optional local-dev only: preload graphics RCC into MEMFS.
  # Release /play gets art from the CDN tip + OPFS (do not inflate tibia.data).
  # Opt in: WASM_PRELOAD_GRAPHICS_RCC=1 make stock wasm build
  set(TIBIA_WASM_GRAPHICS_RCC "" CACHE FILEPATH "graphics_resources.rcc to preload into the wasm build")
  if(TIBIA_WASM_GRAPHICS_RCC)
    target_link_options(tibia PRIVATE "SHELL:--preload-file ${TIBIA_WASM_GRAPHICS_RCC}@/data/graphics_resources.rcc")
  endif()
  # Optional desktop extras (CIP assets come from the tip / OPFS, not tibia.data).
  foreach(_rel IN ITEMS "config.ini")
    if(EXISTS "${TIBIA_ROOT}/${_rel}")
      target_link_options(tibia PRIVATE "SHELL:--preload-file ${TIBIA_ROOT}/${_rel}@/${_rel}")
    endif()
  endforeach()
  # Match Qt wasm kit EH ABI (wasm-exceptions + SUPPORT_LONGJMP=wasm). The game
  # connections are QWebSockets (Crystal WS :7174), linked above.
  if(TIBIA_WITH_GAMECORE)
    if(WASM_USE_WASM_EXCEPTIONS)
      target_compile_options(tibia PRIVATE "SHELL:-fwasm-exceptions")
      target_link_options(tibia PRIVATE
        "SHELL:-fwasm-exceptions"
        "SHELL:-sSUPPORT_LONGJMP=wasm")
    else()
      target_compile_options(tibia PRIVATE "SHELL:-fexceptions")
      target_link_options(tibia PRIVATE
        "SHELL:-fexceptions"
        "SHELL:-sSUPPORT_LONGJMP=emscripten")
    endif()
  endif()
  # Fixed heap (ALLOW_MEMORY_GROWTH=0 — resizable SAB breaks WebGL). Tip-fs
  # one-shot appearances (~5MB) + RCC (~7MB) + protobuf ThingTypes + QML
  # exceeded 512MB shortly after login mask → OOB; 1024MB still OOB'd under
  # IdleOT pack + login-mask sprite/QML traffic. 1536MB leaves headroom.
  set_target_properties(tibia PROPERTIES
    QT_WASM_INITIAL_MEMORY "1536MB")
endif()

# Mobile / HiDPI scaffold (Phase 6): same QML binary; touch + orientation drive
# THudLayout density. Full store packaging is follow-up polish.
if(ANDROID)
  set_target_properties(tibia PROPERTIES
    QT_ANDROID_PACKAGE_SOURCE_DIR "${TIBIA_ROOT}/android/tibia"
    QT_ANDROID_APP_NAME "Tibia"
    QT_ANDROID_VERSION_NAME "0.1.0"
    QT_ANDROID_VERSION_CODE 1)
  target_compile_definitions(tibia PRIVATE TIBIA_MOBILE=1)
endif()
if(IOS)
  set_target_properties(tibia PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER com.tibia.client
    MACOSX_BUNDLE_BUNDLE_NAME Tibia
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "16.0")
  target_compile_definitions(tibia PRIVATE TIBIA_MOBILE=1)
endif()

if(APPLE AND NOT IOS)
  tibia_stage_moltenvk_macos_bundle(tibia)
endif()

# Tip host finalize (STATIC packs link into tibia; no ENABLE_EXPORTS ABI).
qtclient_host_finalize()

if(EMSCRIPTEN)
  # Qt6::Platform ships bare `-sALLOW_MEMORY_GROWTH` (resizable SAB breaks
  # WebGL uploads). Rewrite that INTERFACE entry to =0.
  foreach(_qt_plat IN ITEMS Qt6::Platform Qt6::Core)
    if(NOT TARGET ${_qt_plat})
      continue()
    endif()
    get_target_property(_qt_link_opts ${_qt_plat} INTERFACE_LINK_OPTIONS)
    if(NOT _qt_link_opts)
      continue()
    endif()
    set(_qt_fixed_opts "")
    foreach(_opt IN LISTS _qt_link_opts)
      if(_opt MATCHES "ALLOW_MEMORY_GROWTH")
        list(APPEND _qt_fixed_opts "SHELL:-sALLOW_MEMORY_GROWTH=0")
      else()
        list(APPEND _qt_fixed_opts "${_opt}")
      endif()
    endforeach()
    list(APPEND _qt_fixed_opts "SHELL:-sPTHREAD_POOL_SIZE_STRICT=0")
    # QtWasmHelpers ships STACK_SIZE=5MB; first-frame IdleOT login QML under
    # JSPI OOB'd there — bump on the same INTERFACE rewrite pass (last wins).
    set(_qt_stack_fixed "")
    foreach(_opt IN LISTS _qt_fixed_opts)
      if(_opt MATCHES "STACK_SIZE")
        list(APPEND _qt_stack_fixed "SHELL:-sSTACK_SIZE=16MB")
      else()
        list(APPEND _qt_stack_fixed "${_opt}")
      endif()
    endforeach()
    set(_qt_fixed_opts "${_qt_stack_fixed}")
    set_property(TARGET ${_qt_plat} PROPERTY INTERFACE_LINK_OPTIONS "${_qt_fixed_opts}")
    message(STATUS "tibia wasm: ${_qt_plat} INTERFACE_LINK_OPTIONS → fixed heap + 16MB stack")
  endforeach()
  # Belt-and-suspenders: ensure tibia itself ends with 16MB after Qt macros.
  if(TARGET tibia)
    target_link_options(tibia PRIVATE "SHELL:-sSTACK_SIZE=16MB")
  endif()
endif()
