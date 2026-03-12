cmake_minimum_required(VERSION 3.16)

project(Matrix LANGUAGES C CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)

# Prefer the locally built wlroots tree.
set(WLROOTS_BUILD_DIR "${CMAKE_SOURCE_DIR}/wlroots/build")
set(WLROOTS_PC_DIR "${WLROOTS_BUILD_DIR}/meson-uninstalled")
set(WLROOTS_PROTOCOL_DIR "${WLROOTS_BUILD_DIR}/protocol")
if(EXISTS "${WLROOTS_PC_DIR}/wlroots-0.19-uninstalled.pc")
  set(WLROOTS_PC_NAME "wlroots-0.19-uninstalled")
elseif(EXISTS "${WLROOTS_PC_DIR}/wlroots-0.19.pc")
  set(WLROOTS_PC_NAME "wlroots-0.19")
else()
  set(WLROOTS_PC_NAME "wlroots-0.19")
endif()
if(EXISTS "${WLROOTS_PC_DIR}")
  set(ENV{PKG_CONFIG_PATH} "${WLROOTS_PC_DIR}:$ENV{PKG_CONFIG_PATH}")
endif()

pkg_check_modules(WLROOTS REQUIRED ${WLROOTS_PC_NAME} wayland-server xkbcommon)
pkg_check_modules(GLFW REQUIRED glfw3)
pkg_check_modules(PROTOBUF REQUIRED protobuf)

# Build wlroots locally via meson/ninja, matching the makefile flow.
find_program(MESON_EXECUTABLE meson)
find_program(NINJA_EXECUTABLE ninja)
if(MESON_EXECUTABLE AND NINJA_EXECUTABLE)
  add_custom_command(
    OUTPUT "${WLROOTS_BUILD_DIR}/build.ninja"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/wlroots"
    COMMAND "${MESON_EXECUTABLE}" build
    COMMENT "Configuring wlroots with meson"
  )
  add_custom_command(
    OUTPUT "${WLROOTS_BUILD_DIR}/meson-info/intro-targets.json"
    DEPENDS "${WLROOTS_BUILD_DIR}/build.ninja"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/wlroots"
    COMMAND "${NINJA_EXECUTABLE}" -C build
    COMMENT "Building wlroots"
  )
  add_custom_target(wlroots_build DEPENDS "${WLROOTS_BUILD_DIR}/meson-info/intro-targets.json")
endif()

# Core sources mirroring the makefile's matrix target.
set(MATRIX_SOURCES
  src/main.cpp
  src/wayland/wlr_compositor.cpp
  src/wayland/WaylandApp.cpp
  src/wayland/pointer.cpp
  src/wayland/keyboard.cpp
  src/ControlMappings.cpp
  src/Config.cpp
  src/systems/Player.cpp
  src/MultiPlayer/Server.cpp
  src/MultiPlayer/Client.cpp
  src/MultiPlayer/Gui.cpp
  src/systems/Light.cpp
  src/components/Light.cpp
  src/systems/Boot.cpp
  src/components/Bootable.cpp
  src/IndexPool.cpp
  src/WindowManager/Space.cpp
  src/systems/Move.cpp
  src/systems/ApplyTranslation.cpp
  src/systems/Derivative.cpp
  src/systems/Update.cpp
  src/systems/Intersections.cpp
  src/systems/Scripts.cpp
  src/components/Scriptable.cpp
  src/components/Parent.cpp
  src/components/RotateMovement.cpp
  src/components/Lock.cpp
  src/components/Key.cpp
  src/systems/KeyAndLock.cpp
  src/systems/Door.cpp
  src/systems/ApplyRotation.cpp
  src/persister.cpp
  src/engineGui.cpp
  src/entity.cpp
  src/renderer.cpp
  src/shader.cpp
  src/texture.cpp
  src/world.cpp
  src/camera.cpp
  src/api.cpp
  src/controls.cpp
  src/app.cpp
  src/WindowManager/WindowManager.cpp
  src/logger.cpp
  src/engine.cpp
  src/cube.cpp
  src/chunk.cpp
  src/mesher.cpp
  src/loader.cpp
  src/utility.cpp
  src/blocks.cpp
  src/dynamicObject.cpp
  src/assets.cpp
  src/model.cpp
  src/mesh.cpp
  src/Voxel/VoxelSpace.cpp
  src/imgui/imgui.cpp
  src/imgui/imgui_draw.cpp
  src/imgui/imgui_impl_opengl3.cpp
  src/imgui/imgui_widgets.cpp
  src/imgui/imgui_demo.cpp
  src/imgui/imgui_impl_glfw.cpp
  src/imgui/imgui_tables.cpp
  src/enkimi.c
  src/miniz.c
  src/time_utils.cpp
  src/api.pb.cc
  src/glad.c
  src/glad_glx.c
  tracy/public/TracyClient.cpp
)

file(GLOB SQLITE_SOURCES CONFIGURE_DEPENDS "src/sqlite/*.cpp")
list(APPEND MATRIX_SOURCES ${SQLITE_SOURCES})

add_executable(matrix ${MATRIX_SOURCES})

if(TARGET wlroots_build)
  add_dependencies(matrix wlroots_build)
endif()

set_target_properties(matrix PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)

target_include_directories(matrix PRIVATE
  include
  include/imgui
  tracy/public
  /usr/local/include
  ${WLROOTS_INCLUDE_DIRS}
  ${GLFW_INCLUDE_DIRS}
  ${PROTOBUF_INCLUDE_DIRS}
  ${WLROOTS_PROTOCOL_DIR}
)

target_compile_definitions(matrix PRIVATE
  WLR_USE_UNSTABLE
)

# Match the makefile's Arch/Void fmt handling.
if(EXISTS "/etc/os-release")
  file(READ "/etc/os-release" OS_RELEASE_CONTENT)
  string(FIND "${OS_RELEASE_CONTENT}" "void" OS_RELEASE_HAS_VOID)
  if(NOT OS_RELEASE_HAS_VOID EQUAL -1)
    target_compile_definitions(matrix PRIVATE SPDLOG_FMT_EXTERNAL)
  endif()
endif()

target_compile_options(matrix PRIVATE -O3 -g)

# Apply loader flags only to the loader-style C sources.
set_source_files_properties(
  src/enkimi.c
  src/miniz.c
  PROPERTIES COMPILE_OPTIONS "-march=native;-funroll-loops"
)

target_link_directories(matrix PRIVATE
  ${WLROOTS_LIBRARY_DIRS}
  ${CMAKE_SOURCE_DIR}/lib
)

set(WLROOTS_RPATH_1 "${WLROOTS_BUILD_DIR}")
set(WLROOTS_RPATH_2 "${WLROOTS_BUILD_DIR}/subprojects/wayland/src")
target_link_options(matrix PRIVATE
  "-Wl,-rpath,${WLROOTS_RPATH_1}"
  "-Wl,-rpath,${WLROOTS_RPATH_2}"
  "-Wl,--as-needed"
)

target_link_libraries(matrix PRIVATE
  ${WLROOTS_LIBRARIES}
  ${GLFW_LIBRARIES}
  ${PROTOBUF_LIBRARIES}
  zmq
  X11
  Xcomposite
  Xtst
  Xext
  Xfixes
  pixman-1
  spdlog
  fmt
  GL
  pthread
  assimp
  sqlite3
  EGL
  GLESv2
)
