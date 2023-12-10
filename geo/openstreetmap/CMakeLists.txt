cmake_minimum_required(VERSION 3.10)
project(CityOfCybernetics)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
find_package(Osmium REQUIRED)
find_package(EXPAT MODULE REQUIRED)
find_package(ZLIB REQUIRED)
find_package(BZip2 REQUIRED)

if(NOT OSMIUM_FOUND)
    message(WARNING "Libosmium not found!\n")
endif()

file(GLOB PROTO_FILES ../../src/*.pb.cc)

add_executable(worldGen worldGen.cpp ${PROTO_FILES})


include_directories(${ZMQ_INCLUDE_DIRS})
include_directories(${OSMIUM_INCLUDE_DIRS})
include_directories(${EXPAT_INCLUDE_DIRS})
include_directories (../../include)
include_directories(./include)

target_link_libraries(worldGen PRIVATE EXPAT::EXPAT ZLIB::ZLIB BZip2::BZip2 zmq protobuf)
