# C++ libraries from vcpkg (see vcpkg.json). Qt comes from `make qt`.
# Provides INTERFACE target tibia_thirdparty for engine/tibia link lines.
# CIP prefers Qt/stdlib for JSON, UUID, threads, hashing, UTF-8, and formatting.

find_package(ZLIB REQUIRED)
find_package(LibLZMA REQUIRED)
find_package(Threads REQUIRED)
# CipSoft tip ships spdlog (+ private fmt). qtclient Logger stays the public façade.
find_package(spdlog CONFIG REQUIRED)
find_package(Ogg CONFIG REQUIRED)
find_package(Vorbis CONFIG REQUIRED)

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
  LibLZMA::LibLZMA
  Threads::Threads
  spdlog::spdlog
  Ogg::ogg
  Vorbis::vorbis
  Vorbis::vorbisfile
)
target_compile_definitions(tibia_thirdparty INTERFACE TIBIA_NO_OPENSSL)
