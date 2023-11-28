cmake_minimum_required(VERSION 3.10)
project(CityOfCybernetics)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
find_package(Osmium REQUIRED)
find_package(EXPAT MODULE REQUIRED)
find_package(ZLIB REQUIRED)
find_package(BZip2 REQUIRED)


if(NOT OSMIUM_FOUND)
    message(WARNING "Libosmium not found!\n")
endif()


add_executable(worldGen worldGen.cpp)

include_directories(${OSMIUM_INCLUDE_DIRS})
include_directories(${EXPAT_INCLUDE_DIRS})

target_link_libraries(worldGen PRIVATE EXPAT::EXPAT ZLIB::ZLIB BZip2::BZip2)
