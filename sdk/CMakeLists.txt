# qtclient pack SDK (QtClientSdk) — what a pack needs to build, and nothing else.
#
# Installs:
#   include/qtclient/abi/*.h                       host ABI (the only headers a pack may use)
#   lib/cmake/QtClientSdk/QtClientSdkConfig.cmake  find_package(QtClientSdk) — records the
#                                                  ABI version, hostBuildId, Qt version + kit
#   lib/cmake/QtClientSdk/QtClientMod.cmake        qtclient_add_mod / qtclient_mod_install
#   lib/cmake/QtClientSdk/GeneratePackQmldir.cmake
#   share/qtclient-sdk/sdk.json                    the same facts for scripts
#   share/qtclient-sdk/scripts/                    tip merge helpers (merge-pack-into-tip.sh, …)
#
# Standalone (no host build; only configures, compiles nothing):
#   cmake -S apps/qtclient/sdk -B build/qtclient-sdk-build -DCMAKE_PREFIX_PATH=<Qt kit>
#         [-DCMAKE_TOOLCHAIN_FILE=<wasm Qt>/lib/cmake/Qt6/qt.toolchain.cmake]
#   cmake --install build/qtclient-sdk-build --prefix <sdk dir>
# The host build can also add_subdirectory(sdk) and install it (component `sdk`).
# Lifecycle: scripts/lifecycle/build/sdk.sh (make stock sdk build).
cmake_minimum_required(VERSION 3.22)
if(NOT DEFINED PROJECT_NAME)
  project(qtclient-sdk LANGUAGES CXX)
endif()

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

get_filename_component(_sdk_qtclient_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(_sdk_abi_dir "${_sdk_qtclient_root}/include/qtclient/abi")

# ABI version = QTCLIENT_HOST_API_V1_VERSION (the package version's major).
file(STRINGS "${_sdk_abi_dir}/host_api_v1.h" _sdk_abi_line
  REGEX "QTCLIENT_HOST_API_V1_VERSION[ \t]*=[ \t]*[0-9]+")
string(REGEX REPLACE ".*QTCLIENT_HOST_API_V1_VERSION[ \t]*=[ \t]*([0-9]+).*" "\\1"
  QTCLIENT_SDK_ABI_VERSION "${_sdk_abi_line}")
if(NOT QTCLIENT_SDK_ABI_VERSION)
  message(FATAL_ERROR "qtclient-sdk: QTCLIENT_HOST_API_V1_VERSION not found in host_api_v1.h")
endif()

# hostBuildId: the qtclient commit (same value the tip bakes into host_build_id.cpp).
set(QTCLIENT_SDK_HOST_BUILD_ID "${QTCLIENT_HOST_BUILD_ID}" CACHE STRING "hostBuildId recorded in the SDK")
if(NOT QTCLIENT_SDK_HOST_BUILD_ID AND DEFINED ENV{QTCLIENT_HOST_BUILD_ID})
  set(QTCLIENT_SDK_HOST_BUILD_ID "$ENV{QTCLIENT_HOST_BUILD_ID}")
endif()
if(NOT QTCLIENT_SDK_HOST_BUILD_ID)
  execute_process(COMMAND git -C "${_sdk_qtclient_root}" rev-parse HEAD
    OUTPUT_VARIABLE QTCLIENT_SDK_HOST_BUILD_ID OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()
if(NOT QTCLIENT_SDK_HOST_BUILD_ID)
  set(QTCLIENT_SDK_HOST_BUILD_ID "unknown")
endif()

# Qt: the kit packs must build with (a MODULE shares the tip's Qt; GuiPrivate is version-locked).
find_package(Qt6 6.10 REQUIRED COMPONENTS Core)
set(QTCLIENT_SDK_QT_VERSION "${Qt6_VERSION}")
get_filename_component(QTCLIENT_SDK_QT_PREFIX "${Qt6_DIR}/../../.." ABSOLUTE)
get_target_property(_sdk_qt_type Qt6::Core TYPE)
if(_sdk_qt_type STREQUAL "STATIC_LIBRARY")
  set(QTCLIENT_SDK_QT_STATIC ON)
else()
  set(QTCLIENT_SDK_QT_STATIC OFF)
endif()
if(EMSCRIPTEN OR WASM OR CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  set(QTCLIENT_SDK_PLATFORM "wasm")
  set(QTCLIENT_SDK_PACK_MODE "STATIC_ABI")
elseif(QTCLIENT_SDK_QT_STATIC)
  set(QTCLIENT_SDK_PLATFORM "${CMAKE_SYSTEM_NAME}")
  set(QTCLIENT_SDK_PACK_MODE "STATIC_ABI")
else()
  set(QTCLIENT_SDK_PLATFORM "${CMAKE_SYSTEM_NAME}")
  set(QTCLIENT_SDK_PACK_MODE "MODULE")
endif()

set(_sdk_cmake_dest "${CMAKE_INSTALL_LIBDIR}/cmake/QtClientSdk")
set(_sdk_gen "${CMAKE_CURRENT_BINARY_DIR}/qtclient-sdk")
file(MAKE_DIRECTORY "${_sdk_gen}")

set(QTCLIENT_SDK_INCLUDE_INSTALL_DIR "${CMAKE_INSTALL_INCLUDEDIR}")
configure_package_config_file(
  "${CMAKE_CURRENT_LIST_DIR}/QtClientSdkConfig.cmake.in"
  "${_sdk_gen}/QtClientSdkConfig.cmake"
  INSTALL_DESTINATION "${_sdk_cmake_dest}"
  PATH_VARS QTCLIENT_SDK_INCLUDE_INSTALL_DIR)
write_basic_package_version_file("${_sdk_gen}/QtClientSdkConfigVersion.cmake"
  VERSION "${QTCLIENT_SDK_ABI_VERSION}.0.0"
  COMPATIBILITY SameMajorVersion
  ARCH_INDEPENDENT)

string(TIMESTAMP _sdk_built_at "%Y-%m-%dT%H:%M:%SZ" UTC)
if(QTCLIENT_SDK_QT_STATIC)
  set(_sdk_qt_static_json true)
else()
  set(_sdk_qt_static_json false)
endif()
file(WRITE "${_sdk_gen}/sdk.json" "{
  \"schema\": 4,
  \"abiVersion\": ${QTCLIENT_SDK_ABI_VERSION},
  \"hostBuildId\": \"${QTCLIENT_SDK_HOST_BUILD_ID}\",
  \"qtVersion\": \"${QTCLIENT_SDK_QT_VERSION}\",
  \"qtStatic\": ${_sdk_qt_static_json},
  \"platform\": \"${QTCLIENT_SDK_PLATFORM}\",
  \"packMode\": \"${QTCLIENT_SDK_PACK_MODE}\",
  \"sdkInterface\": \"include/qtclient/abi\",
  \"builtAt\": \"${_sdk_built_at}\"
}
")

install(DIRECTORY "${_sdk_abi_dir}/"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/qtclient/abi"
  COMPONENT sdk
  FILES_MATCHING PATTERN "*.h")
install(FILES "${_sdk_qtclient_root}/include/qtclient/README.md"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/qtclient" COMPONENT sdk OPTIONAL)
install(FILES
  "${_sdk_gen}/QtClientSdkConfig.cmake"
  "${_sdk_gen}/QtClientSdkConfigVersion.cmake"
  "${_sdk_qtclient_root}/cmake/QtClientMod.cmake"
  "${_sdk_qtclient_root}/cmake/GeneratePackQmldir.cmake"
  DESTINATION "${_sdk_cmake_dest}" COMPONENT sdk)
install(FILES "${_sdk_gen}/sdk.json"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/qtclient-sdk" COMPONENT sdk)
install(PROGRAMS
  "${_sdk_qtclient_root}/scripts/lifecycle/lib/merge-pack-into-tip.sh"
  "${_sdk_qtclient_root}/scripts/lifecycle/lib/check-tip-pack-presence.sh"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/qtclient-sdk/scripts" COMPONENT sdk)

message(STATUS "qtclient-sdk: abi v${QTCLIENT_SDK_ABI_VERSION} hostBuildId=${QTCLIENT_SDK_HOST_BUILD_ID} "
  "Qt ${QTCLIENT_SDK_QT_VERSION} (${QTCLIENT_SDK_QT_PREFIX}) platform=${QTCLIENT_SDK_PLATFORM} packs=${QTCLIENT_SDK_PACK_MODE}")
