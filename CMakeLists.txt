# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# Auto build number increment
# Read version from file:
file(READ "version.txt" VERSION_STRING)
# Remove whitespace characters (like newline):
string(STRIP ${VERSION_STRING} VERSION_STRING)
# Match anything before build metadata (+ character) and save it as VER_NUM (usual regex would be: /^(.*?)\+/ but this is cmake):
string(REGEX MATCHALL "^(\.*)\\+" VER_NUM ${VERSION_STRING} )
# Match last number and save it as BUILD_NUM:
string(REGEX MATCHALL "([0-9]+)$" BUILD_NUM ${VERSION_STRING} )
# Increase build number:
math(EXPR BUILD_NUM "${BUILD_NUM}+1")
# Concatenate back to one string:
string(CONCAT VERSION_STRING ${VER_NUM} ${BUILD_NUM})
# Print info and save to file:
#message(NOTICE "New build number: ${VERSION_STRING}")
file(WRITE "version.txt" ${VERSION_STRING})


# For mocking on linux host
# set(COMPONENTS main)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)
project(esp32-zodiac-tri-expert)
