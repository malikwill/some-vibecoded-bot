cmake_minimum_required(VERSION 3.21)
project(MacroBot VERSION 1.0.0)

# Expect GEODE_SDK to be set as an environment variable, pointing to a clone
# of https://github.com/geode-sdk/geode (this is done automatically by the
# geode-sdk/build-geode-mod GitHub Action, see .github/workflows/build.yml)
if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Environment variable GEODE_SDK is not defined. Point it at your local Geode SDK checkout, or build via the provided GitHub Action.")
else()
    message(STATUS "Found Geode SDK: $ENV{GEODE_SDK}")
    set(GEODE_SDK $ENV{GEODE_SDK})
endif()

add_subdirectory(${GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

setup_geode_mod(${PROJECT_NAME})
