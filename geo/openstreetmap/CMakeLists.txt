cmake_minimum_required(VERSION 3.10)
project(CityOfCybernetics)
add_executable(worldGen worldGen.cpp)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
find_package(Osmium REQUIRED)

if(NOT OSMIUM_FOUND)
    message(WARNING "Libosmium not found!\n")
endif()

include_directories(${OSMIUM_INCLUDE_DIRS})
