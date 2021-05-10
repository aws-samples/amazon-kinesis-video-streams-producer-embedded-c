cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(EXTRA_COMPONENT_DIRS 
    "${CMAKE_CURRENT_BINARY_DIR}/components/c-utility"
    "${CMAKE_CURRENT_BINARY_DIR}/components/kvs-embedded-c"
    "${CMAKE_CURRENT_BINARY_DIR}/components/llhttp"
    "${CMAKE_CURRENT_BINARY_DIR}/components/parson"
    "${CMAKE_CURRENT_BINARY_DIR}/components/samples-common"
)

project(kvs-esp32)