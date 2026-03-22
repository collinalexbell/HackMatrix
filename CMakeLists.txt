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
find_package(Git QUIET)
find_program(PROTOC_EXECUTABLE protoc REQUIRED)

# Prefer the locally built wlroots tree.
set(WLROOTS_DIR "${CMAKE_SOURCE_DIR}/wlroots")
set(WLROOTS_BUILD_DIR "${WLROOTS_DIR}/build")
set(WLROOTS_PC_DIR "${WLROOTS_BUILD_DIR}/meson-uninstalled")
set(WLROOTS_PROTOCOL_DIR "${WLROOTS_BUILD_DIR}/protocol")

set(WLROOTS_PC_NAME "wlroots-0.19")
find_program(MESON_EXECUTABLE meson)
find_program(NINJA_EXECUTABLE ninja)

# Build wlroots locally via meson/ninja, matching the makefile flow, and bootstrap the submodule if missing.
function(build_wlroots_if_needed)
  if(EXISTS "${WLROOTS_PC_DIR}/wlroots-0.19-uninstalled.pc" OR EXISTS "${WLROOTS_PC_DIR}/wlroots-0.19.pc")
    return()
  endif()

  if(NOT GIT_FOUND)
    message(FATAL_ERROR "Git is required to initialize the wlroots submodule automatically.")
  endif()
  if(NOT MESON_EXECUTABLE OR NOT NINJA_EXECUTABLE)
    message(FATAL_ERROR "Meson and Ninja are required to build wlroots automatically.")
  endif()

  message(STATUS "Initializing wlroots submodule")
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" submodule update --init --recursive wlroots
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE WLROOTS_GIT_RESULT
  )
  if(NOT WLROOTS_GIT_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to initialize wlroots submodule (exit code ${WLROOTS_GIT_RESULT}).")
  endif()

  message(STATUS "Configuring wlroots with meson")
  execute_process(
    COMMAND "${MESON_EXECUTABLE}" build
    WORKING_DIRECTORY "${WLROOTS_DIR}"
    RESULT_VARIABLE WLROOTS_MESON_RESULT
  )
  if(NOT WLROOTS_MESON_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to configure wlroots with meson (exit code ${WLROOTS_MESON_RESULT}).")
  endif()

  message(STATUS "Building wlroots with ninja")
  execute_process(
    COMMAND "${NINJA_EXECUTABLE}" -C build
    WORKING_DIRECTORY "${WLROOTS_DIR}"
    RESULT_VARIABLE WLROOTS_NINJA_RESULT
  )
  if(NOT WLROOTS_NINJA_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to build wlroots with ninja (exit code ${WLROOTS_NINJA_RESULT}).")
  endif()
endfunction()

build_wlroots_if_needed()

if(EXISTS "${WLROOTS_PC_DIR}/wlroots-0.19-uninstalled.pc")
  set(WLROOTS_PC_NAME "wlroots-0.19-uninstalled")
elseif(EXISTS "${WLROOTS_PC_DIR}/wlroots-0.19.pc")
  set(WLROOTS_PC_NAME "wlroots-0.19")
endif()
if(EXISTS "${WLROOTS_PC_DIR}")
  set(ENV{PKG_CONFIG_PATH} "${WLROOTS_PC_DIR}:$ENV{PKG_CONFIG_PATH}")
endif()

pkg_check_modules(WLROOTS REQUIRED ${WLROOTS_PC_NAME} wayland-server xkbcommon)
pkg_check_modules(GLFW REQUIRED glfw3)
pkg_check_modules(PROTOBUF REQUIRED protobuf)

set(PROTO_FILES
  "protos/api.proto"
)
set(PROTO_ABSOLUTE_FILES)
foreach(PROTO_FILE IN LISTS PROTO_FILES)
  list(APPEND PROTO_ABSOLUTE_FILES "${CMAKE_SOURCE_DIR}/${PROTO_FILE}")
endforeach()
set(PROTO_CPP_OUTPUT_DIR "${CMAKE_BINARY_DIR}/generated")
set(PROTO_CPP_SOURCE "${PROTO_CPP_OUTPUT_DIR}/protos/api.pb.cc")
set(PROTO_CPP_HEADER "${PROTO_CPP_OUTPUT_DIR}/protos/api.pb.h")
set(PROTO_TMP_OUTPUT_DIR "${CMAKE_BINARY_DIR}/generated-tmp")
set(PROTO_PYTHON_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/client_libs/python/hackMatrix")
set(PROTO_PYTHON_MODULE "${PROTO_PYTHON_OUTPUT_DIR}/protos/api_pb2.py")
set(PROTO_HASH_FILE "${CMAKE_BINARY_DIR}/generated/proto-inputs.sha256")
set(PROTO_STAMP_FILE "${CMAKE_BINARY_DIR}/generated/protos.stamp")
set(PROTO_GENERATOR_SCRIPT
  "${CMAKE_BINARY_DIR}/generated/generate_protos_if_changed.cmake")

string(JOIN "\"\n  \"" PROTO_ABSOLUTE_FILES_LITERAL ${PROTO_ABSOLUTE_FILES})

set(PROTO_GENERATOR_SCRIPT_TEMPLATE [=[
set(PROTOC_EXECUTABLE "@PROTOC_EXECUTABLE@")
set(CMAKE_COMMAND_PATH "@CMAKE_COMMAND@")
set(CMAKE_SOURCE_DIR_PATH "@CMAKE_SOURCE_DIR@")
set(PROTO_CPP_OUTPUT_DIR "@PROTO_CPP_OUTPUT_DIR@")
set(PROTO_CPP_SOURCE "@PROTO_CPP_SOURCE@")
set(PROTO_CPP_HEADER "@PROTO_CPP_HEADER@")
set(PROTO_TMP_OUTPUT_DIR "@PROTO_TMP_OUTPUT_DIR@")
set(PROTO_PYTHON_OUTPUT_DIR "@PROTO_PYTHON_OUTPUT_DIR@")
set(PROTO_PYTHON_MODULE "@PROTO_PYTHON_MODULE@")
set(PROTO_HASH_FILE "@PROTO_HASH_FILE@")
set(PROTO_STAMP_FILE "@PROTO_STAMP_FILE@")
set(PROTO_FILES
  "@PROTO_ABSOLUTE_FILES_LITERAL@"
)

set(PROTO_INPUT_HASH "")
foreach(PROTO_FILE IN LISTS PROTO_FILES)
  file(SHA256 "${PROTO_FILE}" PROTO_FILE_HASH)
  string(APPEND PROTO_INPUT_HASH "${PROTO_FILE}:${PROTO_FILE_HASH}\n")
endforeach()

set(SHOULD_GENERATE TRUE)
if(EXISTS "${PROTO_HASH_FILE}")
  file(READ "${PROTO_HASH_FILE}" PREVIOUS_PROTO_INPUT_HASH)
  if(PREVIOUS_PROTO_INPUT_HASH STREQUAL PROTO_INPUT_HASH
     AND EXISTS "${PROTO_CPP_SOURCE}"
     AND EXISTS "${PROTO_CPP_HEADER}"
     AND EXISTS "${PROTO_PYTHON_MODULE}")
    set(SHOULD_GENERATE FALSE)
  endif()
endif()

if(SHOULD_GENERATE)
  file(MAKE_DIRECTORY "${PROTO_CPP_OUTPUT_DIR}")
  file(MAKE_DIRECTORY "${PROTO_CPP_OUTPUT_DIR}/protos")
  file(MAKE_DIRECTORY "${PROTO_TMP_OUTPUT_DIR}")
  file(MAKE_DIRECTORY "${PROTO_TMP_OUTPUT_DIR}/protos")
  file(MAKE_DIRECTORY "${PROTO_PYTHON_OUTPUT_DIR}/protos")

  execute_process(
    COMMAND
      "${PROTOC_EXECUTABLE}"
      "--proto_path=${CMAKE_SOURCE_DIR_PATH}"
      "--cpp_out=${PROTO_TMP_OUTPUT_DIR}"
      ${PROTO_FILES}
    RESULT_VARIABLE PROTO_CPP_RESULT
  )
  if(NOT PROTO_CPP_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to generate protobuf C++ sources.")
  endif()

  execute_process(
    COMMAND
      "${PROTOC_EXECUTABLE}"
      "--proto_path=${CMAKE_SOURCE_DIR_PATH}"
      "--python_out=${PROTO_TMP_OUTPUT_DIR}"
      ${PROTO_FILES}
    RESULT_VARIABLE PROTO_PYTHON_RESULT
  )
  if(NOT PROTO_PYTHON_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to generate protobuf Python sources.")
  endif()

  execute_process(
    COMMAND
      "${CMAKE_COMMAND_PATH}" -E copy_if_different
      "${PROTO_TMP_OUTPUT_DIR}/protos/api.pb.cc"
      "${PROTO_CPP_SOURCE}"
    RESULT_VARIABLE COPY_CPP_RESULT
  )
  if(NOT COPY_CPP_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to update generated protobuf C++ source.")
  endif()

  execute_process(
    COMMAND
      "${CMAKE_COMMAND_PATH}" -E copy_if_different
      "${PROTO_TMP_OUTPUT_DIR}/protos/api.pb.h"
      "${PROTO_CPP_HEADER}"
    RESULT_VARIABLE COPY_HEADER_RESULT
  )
  if(NOT COPY_HEADER_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to update generated protobuf C++ header.")
  endif()

  execute_process(
    COMMAND
      "${CMAKE_COMMAND_PATH}" -E copy_if_different
      "${PROTO_TMP_OUTPUT_DIR}/protos/api_pb2.py"
      "${PROTO_PYTHON_MODULE}"
    RESULT_VARIABLE COPY_PYTHON_RESULT
  )
  if(NOT COPY_PYTHON_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to update generated protobuf Python module.")
  endif()

  file(WRITE "${PROTO_HASH_FILE}" "${PROTO_INPUT_HASH}")
endif()

file(TOUCH "${PROTO_STAMP_FILE}")
]=])
string(CONFIGURE "${PROTO_GENERATOR_SCRIPT_TEMPLATE}" PROTO_GENERATOR_SCRIPT_CONTENT @ONLY)
file(WRITE "${PROTO_GENERATOR_SCRIPT}" "${PROTO_GENERATOR_SCRIPT_CONTENT}")

add_custom_command(
  OUTPUT
    "${PROTO_STAMP_FILE}"
  BYPRODUCTS
    "${PROTO_CPP_SOURCE}"
    "${PROTO_CPP_HEADER}"
    "${PROTO_PYTHON_MODULE}"
  COMMAND "${CMAKE_COMMAND}" -P "${PROTO_GENERATOR_SCRIPT}"
  DEPENDS
    ${PROTO_ABSOLUTE_FILES}
    "${PROTO_GENERATOR_SCRIPT}"
  COMMENT "Generating protobuf sources"
  VERBATIM
)

set_source_files_properties(
  "${PROTO_CPP_SOURCE}"
  "${PROTO_CPP_HEADER}"
  PROPERTIES GENERATED TRUE
)

add_custom_target(generate_protos DEPENDS
  "${PROTO_STAMP_FILE}"
)

# Build wlroots locally via meson/ninja, matching the makefile flow.
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

# Core sources mirroring the makefile's matrix target without enumerating every
# translation unit manually.
file(GLOB MATRIX_SOURCES CONFIGURE_DEPENDS
  src/*.cpp
  src/*.cc
  src/*.c
  src/components/*.cpp
  src/systems/*.cpp
  src/wayland/*.cpp
  src/MultiPlayer/*.cpp
  src/WindowManager/*.cpp
  src/Voxel/*.cpp
  src/imgui/*.cpp
  src/sqlite/*.cpp
)
file(GLOB TRACY_SOURCES CONFIGURE_DEPENDS tracy/public/*.cpp)
list(APPEND MATRIX_SOURCES "${PROTO_CPP_SOURCE}")
list(APPEND MATRIX_SOURCES ${TRACY_SOURCES})

# Exclude helper binaries and test harnesses that define their own entry point.
list(FILTER MATRIX_SOURCES EXCLUDE REGEX "catch_amalgamated\\.cpp$")
list(FILTER MATRIX_SOURCES EXCLUDE REGEX "diagnosis\\.cpp$")
list(FILTER MATRIX_SOURCES EXCLUDE REGEX "trampoline\\.cpp$")
list(FILTER MATRIX_SOURCES EXCLUDE REGEX "/MultiPlayer/bootServer\\.cpp$")
list(FILTER MATRIX_SOURCES EXCLUDE REGEX "/MultiPlayer/bot\\.cpp$")
list(FILTER MATRIX_SOURCES EXCLUDE REGEX "objectObserver\\.cpp$")
list(REMOVE_DUPLICATES MATRIX_SOURCES)

add_executable(matrix ${MATRIX_SOURCES})

if(TARGET wlroots_build)
  add_dependencies(matrix wlroots_build)
endif()
add_dependencies(matrix generate_protos)

set_target_properties(matrix PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)

target_include_directories(matrix PRIVATE
  include
  "${PROTO_CPP_OUTPUT_DIR}"
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
