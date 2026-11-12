# Desktop qtclient tip: CIP-parity optional frameworks (WebEngine, MoltenVK).
# Wasm / Android / iOS always use inert qmlstubs for QtWebEngine.

include_guard(GLOBAL)

function(tibia_resolve_desktop_webengine out_use_stubs)
  set(_use_stubs ON)
  if(EMSCRIPTEN OR ANDROID OR IOS)
    set(${out_use_stubs} ON PARENT_SCOPE)
    return()
  endif()
  find_package(Qt6 6.10 QUIET COMPONENTS WebEngineQuick WebChannel)
  if(TARGET Qt6::WebEngineQuick AND TARGET Qt6::WebChannel)
    set(_use_stubs OFF)
    message(STATUS "tibia: desktop Qt WebEngineQuick + WebChannel (CIP store/news)")
  else()
    message(STATUS "tibia: Qt WebEngineQuick not found — using qmlstubs.")
    message(STATUS "  CIP parity: rebuild CIP Qt without qtwebengine in QT_SKIP_MODULES")
    message(STATUS "  (see apps/qtclient/tools/qt/README.md#desktop-webengine) then reconfigure.")
  endif()
  set(${out_use_stubs} ${_use_stubs} PARENT_SCOPE)
endfunction()

function(tibia_link_desktop_webengine target use_stubs)
  if(use_stubs)
    if(NOT TARGET tibia_webengineplugin OR NOT TARGET tibia_webchannelplugin)
      message(FATAL_ERROR "tibia: qmlstubs missing but WebEngine stubs were requested")
    endif()
    target_link_libraries(${target} PRIVATE tibia_webengineplugin tibia_webchannelplugin)
    target_compile_definitions(${target} PRIVATE TIBIA_WEBENGINE_STUBS=1)
  else()
    target_link_libraries(${target} PRIVATE Qt6::WebEngineQuick Qt6::WebChannel)
    target_compile_definitions(${target} PRIVATE TIBIA_DESKTOP_WEBENGINE=1)
    if(TIBIA_QT_STATIC)
      foreach(_plugin IN ITEMS Qt6::qtwebenginequickplugin Qt6::qwebenginequickplugin
                               Qt6::webenginequickplugin Qt6::qwebchannelquickplugin)
        if(TARGET ${_plugin})
          target_link_libraries(${target} PRIVATE ${_plugin})
        endif()
      endforeach()
    endif()
  endif()
endfunction()

# Locate MoltenVK dylib + ICD for macOS Vulkan (rendererIndex 7). Returns empty when absent.
function(tibia_find_moltenvk out_dylib out_icd)
  set(_dylib "")
  set(_icd "")
  if(NOT APPLE)
    set(${out_dylib} "" PARENT_SCOPE)
    set(${out_icd} "" PARENT_SCOPE)
    return()
  endif()

  set(_search_roots "")
  if(DEFINED ENV{TIBIA_MOLTENVK_DIR} AND IS_DIRECTORY "$ENV{TIBIA_MOLTENVK_DIR}")
    list(APPEND _search_roots "$ENV{TIBIA_MOLTENVK_DIR}")
  endif()
  if(DEFINED ENV{VULKAN_SDK} AND IS_DIRECTORY "$ENV{VULKAN_SDK}")
    list(APPEND _search_roots
      "$ENV{VULKAN_SDK}/MoltenVK/dylib/macOS"
      "$ENV{VULKAN_SDK}/macOS/lib")
  endif()
  if(DEFINED Qt6_DIR)
    get_filename_component(_qt_prefix "${Qt6_DIR}/../../.." ABSOLUTE)
    list(APPEND _search_roots
      "${_qt_prefix}/Frameworks"
      "${_qt_prefix}/lib"
      "${_qt_prefix}/share/vulkan/icd.d")
  endif()
  list(APPEND _search_roots
    "/opt/homebrew/opt/molten-vk/lib"
    "/opt/homebrew/opt/molten-vk"
    "/usr/local/opt/molten-vk/lib"
    "/usr/local/opt/molten-vk")

  foreach(_root IN LISTS _search_roots)
    if(NOT _dylib)
      file(GLOB _candidates
        "${_root}/libMoltenVK.dylib"
        "${_root}/lib/libMoltenVK.dylib"
        "${_root}/MoltenVK.framework/MoltenVK")
      if(_candidates)
        list(GET _candidates 0 _dylib)
      endif()
    endif()
    if(NOT _icd)
      foreach(_name IN ITEMS MoltenVK_icd.json vk_icdMoltenVK.json)
        foreach(_subdir IN ITEMS "" "etc/vulkan/icd.d" "share/vulkan/icd.d" "lib")
          if(_subdir STREQUAL "")
            set(_cand "${_root}/${_name}")
          else()
            set(_cand "${_root}/${_subdir}/${_name}")
          endif()
          if(EXISTS "${_cand}")
            set(_icd "${_cand}")
            break()
          endif()
        endforeach()
        if(_icd)
          break()
        endif()
      endforeach()
    endif()
  endforeach()

  set(${out_dylib} "${_dylib}" PARENT_SCOPE)
  set(${out_icd} "${_icd}" PARENT_SCOPE)
endfunction()

function(tibia_stage_moltenvk_macos_bundle target)
  if(NOT APPLE)
    return()
  endif()
  tibia_find_moltenvk(_dylib _icd)
  if(NOT _dylib)
    message(STATUS "tibia: MoltenVK not found — Vulkan renderer needs Frameworks/libMoltenVK.dylib")
    message(STATUS "  Set TIBIA_MOLTENVK_DIR or install MoltenVK; package_desktop.py copies when present.")
    return()
  endif()

  set(_frameworks "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Frameworks")
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_frameworks}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_dylib}" "${_frameworks}/libMoltenVK.dylib"
    COMMENT "Stage MoltenVK into client.app Frameworks"
    VERBATIM)

  # Always emit a tip-local ICD so library_path is relative to Frameworks/ (Homebrew
  # ICD files often point at Cellar absolute paths).
  set(_gen_icd "${CMAKE_CURRENT_BINARY_DIR}/MoltenVK_icd.json")
  file(WRITE "${_gen_icd}"
    "{\n  \"file_format_version\": \"1.0.0\",\n  \"ICD\": {\n    \"library_path\": \"libMoltenVK.dylib\",\n    \"api_version\": \"1.0.0\"\n  }\n}\n")
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_gen_icd}" "${_frameworks}/MoltenVK_icd.json"
    VERBATIM)

  target_compile_definitions(${target} PRIVATE TIBIA_MOLTENVK_BUNDLED=1)
  message(STATUS "tibia: MoltenVK POST_BUILD → Frameworks (from ${_dylib})")
endfunction()
