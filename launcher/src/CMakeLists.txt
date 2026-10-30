find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

# Launcher is added before client/tibia (which sets CMAKE_AUTOMOC for tibia).
set(CMAKE_AUTOMOC ON)

get_filename_component(TIBIA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

# Stock CipSoft identity; first launcher pack with brand.cmake or mod.json overrides.
set(TIBIA_PRODUCT_NAME "Tibia")
set(TIBIA_PRODUCT_ORG "CipSoft")
set(TIBIA_PRODUCT_BUNDLE_ID "com.tibia.launcher")
set(TIBIA_PRODUCT_URL "https://www.tibia.com/")
set(TIBIA_DEFAULT_CDN "")
set(TIBIA_CLIENT_PACKAGE_DIR "Tibia.app")

set(_launcher_mod_art "")
file(GLOB _launcher_mod_dirs LIST_DIRECTORIES true "${TIBIA_ROOT}/launcher/mods/*")
foreach(_d IN LISTS _launcher_mod_dirs)
  get_filename_component(_n "${_d}" NAME)
  if(_n MATCHES "^_")
    continue()
  endif()
  if(EXISTS "${_d}/brand.cmake")
    include("${_d}/brand.cmake")
  endif()
  if(_launcher_mod_art STREQUAL "" AND EXISTS "${_d}/images/launcher")
    set(_launcher_mod_art "${_d}")
  endif()
endforeach()

qt_add_executable(launcher
  main.cpp
  package.cpp
  package.h
  unpack.cpp
  unpack.h
  window.cpp
  window.h
  "${TIBIA_ROOT}/client/tibia/app/hints.cpp"
  "${TIBIA_ROOT}/client/tibia/app/hints.h"
  lzma/LzmaDec.c
)

# CIP chrome: launcher/upstream (vendored by launcher sync).
# Stock forks: launcher/overrides/<rel>.
# Product art: first launcher/mods/<pack>/ with images/launcher.
set(_chrome "${TIBIA_ROOT}/launcher/upstream")
set(_overrides "${TIBIA_ROOT}/launcher/overrides")
set(_mod_art "${_launcher_mod_art}")
file(GLOB_RECURSE _chrome_files CONFIGURE_DEPENDS "${_chrome}/*")
set(_launcher_resources)
set(_product_art_names "BackgroundImage.jpg;TibiaLogo_Classic.png;CipSoftLogo.png")
foreach(_file IN LISTS _chrome_files)
  file(RELATIVE_PATH _rel "${_chrome}" "${_file}")
  get_filename_component(_name "${_file}" NAME)
  set(_src "${_file}")
  set(_override "${_overrides}/${_rel}")
  if(NOT EXISTS "${_override}" AND _name IN_LIST _product_art_names AND NOT _mod_art STREQUAL "")
    set(_override "${_mod_art}/images/launcher/${_name}")
  endif()
  if(EXISTS "${_override}")
    set(_src "${_override}")
  endif()
  set_source_files_properties("${_src}" PROPERTIES QT_RESOURCE_ALIAS "${_rel}")
  list(APPEND _launcher_resources "${_src}")
endforeach()
foreach(_name IN ITEMS BackgroundImage.jpg TibiaLogo_Classic.png CipSoftLogo.png)
  set(_override "${_overrides}/images/launcher/${_name}")
  if(NOT EXISTS "${_override}" AND NOT _mod_art STREQUAL "")
    set(_override "${_mod_art}/images/launcher/${_name}")
  endif()
  if(NOT EXISTS "${_override}")
    continue()
  endif()
  set(_alias "images/launcher/${_name}")
  set(_already FALSE)
  foreach(_res IN LISTS _launcher_resources)
    get_source_file_property(_res_alias "${_res}" QT_RESOURCE_ALIAS)
    if(_res_alias STREQUAL _alias)
      set(_already TRUE)
      break()
    endif()
  endforeach()
  if(NOT _already)
    set_source_files_properties("${_override}" PROPERTIES QT_RESOURCE_ALIAS "${_alias}")
    list(APPEND _launcher_resources "${_override}")
  endif()
endforeach()
# CIP-shaped hints.json (title + richText with /images/news/*).
set(_hints_json "${TIBIA_ROOT}/data/hints.json")
if(EXISTS "${_hints_json}")
  set_source_files_properties("${_hints_json}" PROPERTIES QT_RESOURCE_ALIAS "hints.json")
  list(APPEND _launcher_resources "${_hints_json}")
endif()

if(_launcher_resources)
  qt_add_resources(launcher launcher_assets PREFIX "/" FILES ${_launcher_resources})
endif()

target_include_directories(launcher PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/lzma"
  "${TIBIA_ROOT}/client/tibia"
  "${TIBIA_ROOT}/shared/src")
target_link_libraries(launcher PRIVATE Qt6::Widgets Qt6::Network qtclient_shared)
# QStringLiteral(TIBIA_LAUNCHER_VERSION) in main.cpp / window.cpp (wasm + desktop).
set(_launcher_version "dev")
execute_process(
  COMMAND git -C "${TIBIA_ROOT}" rev-parse --short HEAD
  OUTPUT_VARIABLE _launcher_version
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET)
target_compile_definitions(launcher PRIVATE
  "TIBIA_PRODUCT_NAME=\"${TIBIA_PRODUCT_NAME}\""
  "TIBIA_PRODUCT_ORG=\"${TIBIA_PRODUCT_ORG}\""
  "TIBIA_PRODUCT_BUNDLE_ID=\"${TIBIA_PRODUCT_BUNDLE_ID}\""
  "TIBIA_PRODUCT_URL=\"${TIBIA_PRODUCT_URL}\""
  "TIBIA_DEFAULT_CDN=\"${TIBIA_DEFAULT_CDN}\""
  "TIBIA_CLIENT_PACKAGE_DIR=\"${TIBIA_CLIENT_PACKAGE_DIR}\""
  "TIBIA_LAUNCHER_VERSION=\"${_launcher_version}\"")

if(APPLE)
  set_target_properties(launcher PROPERTIES
    MACOSX_BUNDLE ON
    MACOSX_BUNDLE_GUI_IDENTIFIER "${TIBIA_PRODUCT_BUNDLE_ID}"
    MACOSX_BUNDLE_BUNDLE_NAME "${TIBIA_PRODUCT_NAME}"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/macos/Info.plist.in")
endif()
# Match tibia: package_launcher --bin build/<preset>/bin finds the Soft launcher.
if(TOGGLE_BIN_FOLDER)
  set_target_properties(launcher PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
  if(APPLE)
    set_target_properties(launcher PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
  endif()
endif()
