cmake_minimum_required(VERSION 3.10)
project(matrix)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_COMPILER g++)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -funroll-loops")

set(OpenGL_GL_PREFERENCE GLVND)
# Libraries
find_package(OpenGL REQUIRED COMPONENTS GLX)
find_package(X11 REQUIRED COMPONENTS Xfixes Xext Xcomposite Xxf86vm)
find_package(Protobuf REQUIRED)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(Threads REQUIRED)
find_package(assimp REQUIRED) 
find_package(SQLite3 REQUIRED)

## load in pkg-config support
find_package(PkgConfig)
## use pkg-config to get hints for 0mq locations
pkg_check_modules(PC_ZeroMQ QUIET zmq)

## use the hint from above to find the location of libzmq
find_library(ZeroMQ_LIBRARY
        NAMES zmq
        PATHS ${PC_ZeroMQ_LIBRARY_DIRS}
        )



# Include directories
include_directories(
    include 
    include/imgui
    /usr/local/include
)


# Source files
file(GLOB SOURCES
    "src/*.cpp"
    "src/*.c"
    "src/systems/*.cpp"
    "src/components/*.cpp"
    "src/WindowManager/*.cpp"
    "src/imgui/*.cpp"
)

list(APPEND SOURCES "src/glad.c" "src/glad_glx.c")
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/catch_amalgamated.cpp")
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/x-raise.c")
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/trampoline.cpp")

# SQLite sources
file(GLOB SQLITE_SOURCES "src/sqlite/*.cpp")

# ImGui sources
set(IMGUI_SOURCES
    src/imgui/imgui.cpp
    src/imgui/imgui_draw.cpp 
    src/imgui/imgui_impl_opengl3.cpp
    src/imgui/imgui_widgets.cpp
    src/imgui/imgui_demo.cpp
    src/imgui/imgui_impl_glfw.cpp
    src/imgui/imgui_tables.cpp
)

# Protobuf generation
set(PROTO_DIR protos)
file(GLOB PROTO_FILES "${PROTO_DIR}/*.proto")
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/..)

# Executable
add_executable(matrix ${SOURCES} ${SQLITE_SOURCES} ${IMGUI_SOURCES} ${PROTO_SRCS})

target_link_libraries(matrix 
    protobuf::libprotobuf
    spdlog::spdlog
    fmt::fmt
    Threads::Threads
    assimp::assimp
    SQLite::SQLite3
    glfw
    OpenGL::OpenGL
    OpenGL::GLX
    ${X11_LIBRARIES}
    ${X11_Xfixes_LIB}
    ${X11_Xext_LIB}
    ${X11_Xcomposite_LIB}
    ${X11_Xxf86vm_LIB}
    ${ZeroMQ_LIBRARY}
)

# Trampoline executable
add_executable(trampoline src/trampoline.cpp)

# x-raise utility
add_executable(x-raise src/x-raise.c)
target_link_libraries(x-raise X11)

