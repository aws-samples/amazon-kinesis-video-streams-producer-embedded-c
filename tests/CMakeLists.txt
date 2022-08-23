cmake_minimum_required(VERSION 3.2.0)
project(embedded_producer_tests)

enable_testing()

set(LIB_DIR ${CMAKE_SOURCE_DIR}/src)
set(LIB_PRV_INC
    ${LIB_DIR}/source
)

add_executable(${PROJECT_NAME}
    errors_test.cpp
    http_parser_adapter_test.cpp
    nalu_test.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE ${LIB_PRV_INC})
target_link_libraries(${PROJECT_NAME}
    kvs-embedded-c
    gtest_main
)