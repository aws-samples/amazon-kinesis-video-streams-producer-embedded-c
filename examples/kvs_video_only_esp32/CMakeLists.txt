cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set (EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_BINARY_DIR}/components/kvs-producer-rtos"
                          "${CMAKE_CURRENT_BINARY_DIR}/components/corehttp"
                          "${CMAKE_CURRENT_BINARY_DIR}/components/parson"
                          )

project(kvs_esp_video_only)