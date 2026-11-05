cmake_minimum_required(VERSION 3.16)
project(Strata VERSION 1.0.0 LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(STRATA_BUILD_TESTS "Build the unit tests" ON)
option(STRATA_FETCH_GLFW "Download and build GLFW when no system copy is found" ON)
option(STRATA_WERROR "Treat compiler warnings as errors" OFF)
set(STRATA_SANITIZE "" CACHE STRING "Sanitizers to enable, e.g. address,undefined or thread")
# Web build only: use WebAssembly threads (needs a cross-origin isolated page,
# i.e. COOP/COEP headers). The single-threaded build runs everywhere.
option(STRATA_WEB_THREADS "Web build: run world generation on worker threads" OFF)

if(EMSCRIPTEN)
  set(STRATA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  if(STRATA_WEB_THREADS)
    add_compile_options(-pthread)
    add_link_options(-pthread)
  endif()
else()
  find_package(Threads REQUIRED)
endif()

# ---------------------------------------------------------------------------
# Shared compiler settings
# ---------------------------------------------------------------------------
add_library(strata_options INTERFACE)
if(MSVC)
  target_compile_options(strata_options INTERFACE /W4 /permissive- /utf-8 /fp:precise)
  target_compile_definitions(strata_options INTERFACE NOMINMAX _CRT_SECURE_NO_WARNINGS)
  if(STRATA_WERROR)
    target_compile_options(strata_options INTERFACE /WX)
  endif()
else()
  # -ffp-contract=off keeps floating point results identical no matter how the
  # compiler inlines, which world generation relies on at chunk borders.
  target_compile_options(strata_options INTERFACE -Wall -Wextra -Wpedantic -Wshadow -ffp-contract=off)
  if(STRATA_WERROR)
    target_compile_options(strata_options INTERFACE -Werror)
  endif()
  if(STRATA_SANITIZE)
    target_compile_options(strata_options INTERFACE -fsanitize=${STRATA_SANITIZE} -fno-omit-frame-pointer -g)
    target_link_options(strata_options INTERFACE -fsanitize=${STRATA_SANITIZE})
  endif()
endif()

# ---------------------------------------------------------------------------
# Engine core: everything that does not need a window or OpenGL.
# Unit tests link against this library directly.
# ---------------------------------------------------------------------------
add_library(strata_core STATIC
  src/core/image_io.cpp
  src/core/log.cpp
  src/core/noise.cpp
  src/core/thread_pool.cpp
  src/world/block.cpp
  src/world/chunk.cpp
  src/world/items.cpp
  src/world/lighting.cpp
  src/world/mesher.cpp
  src/world/raycast.cpp
  src/world/storage.cpp
  src/world/world.cpp
  src/world/worldgen.cpp
  src/render/atmosphere.cpp
  src/render/textures.cpp
  src/game/player.cpp
  src/game/settings.cpp
)
target_include_directories(strata_core PUBLIC src)
target_link_libraries(strata_core PUBLIC strata_options)
if(NOT EMSCRIPTEN)
  target_link_libraries(strata_core PUBLIC Threads::Threads)
endif()

# ---------------------------------------------------------------------------
# Rendering: OpenGL 3.3 core on the desktop, OpenGL ES 3.0 (WebGL 2) on the web.
# ---------------------------------------------------------------------------
add_library(strata_render STATIC
  src/gl/gl.cpp
  src/gl/gl_util.cpp
  src/gl/shader.cpp
  src/render/font.cpp
  src/render/renderer.cpp
  src/render/shaders.cpp
  src/render/ui.cpp
)
target_link_libraries(strata_render PUBLIC strata_core)

set(STRATA_GAME_SOURCES
  src/main.cpp
  src/game/game.cpp
  src/game/input.cpp
  src/game/screens.cpp
  src/game/touch.cpp
)

if(EMSCRIPTEN)
  # -------------------------------------------------------------------------
  # Browser build (phones, tablets and desktop browsers): WebAssembly + WebGL 2.
  # Produces strata.js / strata.wasm (or strata-mt.* with threads); the page,
  # manifest and service worker come from web/.
  # -------------------------------------------------------------------------
  if(STRATA_WEB_THREADS)
    set(STRATA_WEB_NAME strata-mt)
  else()
    set(STRATA_WEB_NAME strata)
  endif()
  add_executable(strata ${STRATA_GAME_SOURCES} src/platform/window_web.cpp)
  set_target_properties(strata PROPERTIES OUTPUT_NAME ${STRATA_WEB_NAME} SUFFIX ".js")
  target_link_libraries(strata PRIVATE strata_render)
  target_link_options(strata PRIVATE
    -sMIN_WEBGL_VERSION=2
    -sMAX_WEBGL_VERSION=2
    -sALLOW_MEMORY_GROWTH=1
    -sINITIAL_MEMORY=134217728
    -sMAXIMUM_MEMORY=2147483648
    -sFORCE_FILESYSTEM=1
    -sEXIT_RUNTIME=0
    -sENVIRONMENT=web,worker
    -lidbfs.js
    "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/web/pre.js"
  )
  if(STRATA_WEB_THREADS)
    target_link_options(strata PRIVATE -sPTHREAD_POOL_SIZE=4 -Wno-pthreads-mem-growth)
  endif()
  set_property(TARGET strata APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/web/pre.js")
  # Copy the page and its assets next to the build output.
  foreach(asset index.html manifest.webmanifest sw.js icon.svg icon-192.png icon-512.png icon-maskable-512.png)
    add_custom_command(TARGET strata POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/web/${asset}"
              "$<TARGET_FILE_DIR:strata>/${asset}")
  endforeach()
else()
  # -------------------------------------------------------------------------
  # GLFW: prefer a system installation, otherwise fetch the pinned release.
  # -------------------------------------------------------------------------
  find_package(glfw3 3.3 QUIET)
  if(glfw3_FOUND)
    message(STATUS "Strata: using system GLFW ${glfw3_VERSION}")
  elseif(STRATA_FETCH_GLFW)
    message(STATUS "Strata: system GLFW not found, downloading GLFW 3.4")
    include(FetchContent)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(glfw
      URL https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip
      URL_HASH SHA256=b5ec004b2712fd08e8861dc271428f048775200a2df719ccf575143ba749a3e9
    )
    FetchContent_MakeAvailable(glfw)
  else()
    message(FATAL_ERROR "GLFW 3.3+ was not found. Install it (e.g. libglfw3-dev) or enable STRATA_FETCH_GLFW.")
  endif()

  # -------------------------------------------------------------------------
  # The desktop game
  # -------------------------------------------------------------------------
  add_executable(strata ${STRATA_GAME_SOURCES} src/platform/window_glfw.cpp)
  target_link_libraries(strata PRIVATE strata_render glfw)

  # Writes every shader for both GLSL dialects; tools/check_shaders.py
  # validates them with glslangValidator.
  add_executable(strata_shader_dump tools/dump_shaders.cpp)
  target_link_libraries(strata_shader_dump PRIVATE strata_render)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(STRATA_BUILD_TESTS)
  enable_testing()
  add_executable(strata_tests tests/tests.cpp src/game/input.cpp src/game/touch.cpp)
  target_link_libraries(strata_tests PRIVATE strata_render)
  add_test(NAME strata_tests COMMAND strata_tests)
endif()
