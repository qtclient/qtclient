# CIP shared:: — cross-cutting types used by client and launcher.
# FS home mirrors the recovered namespace (contract classes/shared.json).

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)
find_package(ZLIB REQUIRED)

add_library(qtclient_shared STATIC
  src/textbrowserwithnetwork.cpp
  src/textbrowserwithnetwork.h
  src/bundle.cpp
  src/bundle.h
  src/compressionhelper.cpp
  src/compressionhelper.h
  src/decompressionerror.cpp
  src/decompressionerror.h
  src/filesystemhelper.cpp
  src/filesystemhelper.h
  src/hashhelper.cpp
  src/hashhelper.h
  src/localfileinfo.cpp
  src/localfileinfo.h
  src/lzmacompressedreadonlymemoryblob.cpp
  src/lzmacompressedreadonlymemoryblob.h
  src/randomnumbergenerator.cpp
  src/randomnumbergenerator.h
  src/readonlymemoryblob.cpp
  src/readonlymemoryblob.h
  src/rsahelper.cpp
  src/rsahelper.h
  src/xteahelper.cpp
  src/xteahelper.h
  src/zlibbasewrapper.cpp
  src/zlibbasewrapper.h
  src/zlibdeflatewrapper.cpp
  src/zlibdeflatewrapper.h
  src/zlibinflatewrapper.cpp
  src/zlibinflatewrapper.h
  # LZMA SDK decoder (the bundle library's LZMA codec).
  src/lzma/LzmaDec.c
)
set_target_properties(qtclient_shared PROPERTIES AUTOMOC ON)
target_compile_definitions(qtclient_shared PRIVATE QT_NO_KEYWORDS)
target_include_directories(qtclient_shared PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_include_directories(qtclient_shared PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src/lzma")
target_link_libraries(qtclient_shared PUBLIC Qt6::Widgets Qt6::Network ZLIB::ZLIB)
