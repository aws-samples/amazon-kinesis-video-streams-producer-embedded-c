cmake_minimum_required(VERSION 3.2.0)
project(embedded_producer_tests)

enable_testing()

add_executable(${PROJECT_NAME}
    errors_test.cpp
    nalu_test.cpp
)

target_link_libraries(${PROJECT_NAME}
    kvs-embedded-c
    gtest_main
)