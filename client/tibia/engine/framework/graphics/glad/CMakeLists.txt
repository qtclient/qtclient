# Vendored glad 2.0.8 — OpenGL 3.3 core + loader.
# Generated with: python -m glad --api gl:core=3.3 --reproducible --extensions '' c --loader
# Own directory so parent CXX force-includes and Qt flags never touch gl.c.
enable_language(C)
add_library(tibia_glad STATIC src/gl.c)
target_include_directories(tibia_glad PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_definitions(tibia_glad PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
set_target_properties(tibia_glad PROPERTIES
  LINKER_LANGUAGE C
  C_STANDARD 99
  C_EXTENSIONS OFF)
set_source_files_properties(src/gl.c PROPERTIES LANGUAGE C)
if(CMAKE_DL_LIBS)
  target_link_libraries(tibia_glad PUBLIC ${CMAKE_DL_LIBS})
endif()
