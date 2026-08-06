cmake_minimum_required(VERSION 3.25)
project(SmartAutoCheckpoint VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(DEFINED ENV{GEODE_SDK})
    file(TO_CMAKE_PATH "$ENV{GEODE_SDK}" GEODE_SDK)
else()
    set(GEODE_SDK "C:/Users/cesar/Documents/Geode")
endif()

list(APPEND CMAKE_MODULE_PATH "${GEODE_SDK}/cmake")
include(GeodeFile)

add_subdirectory("${GEODE_SDK}" "${CMAKE_BINARY_DIR}/geode")

add_library(SmartAutoCheckpoint SHARED src/main.cpp)
setup_geode_mod(SmartAutoCheckpoint)
