# C++ libraries from vcpkg (see vcpkg.json). Qt comes from `make qt`.
# Provides INTERFACE target tibia_thirdparty for engine/tibia link lines.

find_package(ZLIB REQUIRED)
find_package(PNG REQUIRED)
find_package(LibLZMA REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(utf8cpp CONFIG REQUIRED)
find_package(stduuid CONFIG REQUIRED)
find_package(Threads REQUIRED)

# Header-only ports without (or with minimal) imported targets.
find_path(PHMAP_INCLUDE_DIR NAMES parallel_hashmap/phmap.h REQUIRED)
find_path(BS_THREAD_POOL_INCLUDE_DIR NAMES BS_thread_pool.hpp REQUIRED)

find_package(Protobuf CONFIG REQUIRED)
if(NOT TARGET protobuf::libprotobuf AND TARGET protobuf::protobuf)
  add_library(protobuf::libprotobuf ALIAS protobuf::protobuf)
endif()
if(NOT COMMAND protobuf_generate)
  foreach(_pb_gen IN ITEMS
      "${Protobuf_DIR}/protobuf-generate.cmake"
      "${Protobuf_DIR}/protobuf-module.cmake")
    if(EXISTS "${_pb_gen}")
      include("${_pb_gen}")
      break()
    endif()
  endforeach()
endif()

add_library(tibia_thirdparty INTERFACE)
target_link_libraries(tibia_thirdparty INTERFACE
  ZLIB::ZLIB
  PNG::PNG
  LibLZMA::LibLZMA
  fmt::fmt
  nlohmann_json::nlohmann_json
  utf8cpp::utf8cpp
  Threads::Threads
)
if(TARGET stduuid)
  target_link_libraries(tibia_thirdparty INTERFACE stduuid)
elseif(TARGET stduuid::stduuid)
  target_link_libraries(tibia_thirdparty INTERFACE stduuid::stduuid)
endif()
target_include_directories(tibia_thirdparty INTERFACE
  "${PHMAP_INCLUDE_DIR}"
  "${BS_THREAD_POOL_INCLUDE_DIR}"
)
# code uses #include <uuid.h>; vcpkg installs stduuid/uuid.h
find_path(_STDUUID_UUID_DIR NAMES uuid.h PATH_SUFFIXES stduuid)
if(_STDUUID_UUID_DIR)
  target_include_directories(tibia_thirdparty INTERFACE "${_STDUUID_UUID_DIR}")
endif()
target_compile_definitions(tibia_thirdparty INTERFACE TIBIA_NO_OPENSSL)
