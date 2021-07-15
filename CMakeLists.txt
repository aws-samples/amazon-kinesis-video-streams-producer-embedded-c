cmake_minimum_required(VERSION 3.2.0)
project(kvs-embedded-c)

# Make warning as error
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")

# compiled as C99
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --std=c99")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11")

# Add thirdparty library mbedtls
set(ENABLE_PROGRAMS OFF CACHE BOOL "")
set(ENABLE_TESTING OFF CACHE BOOL "")
set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "")
set(USE_STATIC_MBEDTLS_LIBRARY ON CACHE BOOL "")
add_subdirectory(libraries/3rdparty/mbedtls)

# Add thirdparty library llhttp
add_subdirectory(libraries/3rdparty/llhttp)

# Add thirdparty library parson
set(PARSON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libraries/3rdparty/parson)

add_library(parson
    ${PARSON_DIR}/parson.c
    ${PARSON_DIR}/parson.h
)

target_include_directories(parson PUBLIC
    ${PARSON_DIR}
)

# build azure c shared utility
set(AZURE_C_SHARED_UTILITY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libraries/3rdparty/c-utility)

add_library(aziotsharedutil
    ${AZURE_C_SHARED_UTILITY_DIR}/adapters/lock_pthreads.c
    ${AZURE_C_SHARED_UTILITY_DIR}/src/buffer.c
    ${AZURE_C_SHARED_UTILITY_DIR}/src/consolelogger.c
    ${AZURE_C_SHARED_UTILITY_DIR}/src/crt_abstractions.c
    ${AZURE_C_SHARED_UTILITY_DIR}/src/doublylinkedlist.c
    ${AZURE_C_SHARED_UTILITY_DIR}/src/httpheaders.c
    ${AZURE_C_SHARED_UTILITY_DIR}/src/map.c
    ${AZURE_C_SHARED_UTILITY_DIR}/src/strings.c
    ${AZURE_C_SHARED_UTILITY_DIR}/src/xlogging.c
)

target_include_directories(aziotsharedutil PUBLIC
    ${AZURE_C_SHARED_UTILITY_DIR}/inc
    ${AZURE_C_SHARED_UTILITY_DIR}/deps/azure-macro-utils-c/inc
    ${AZURE_C_SHARED_UTILITY_DIR}/deps/umock-c/inc
)

target_link_libraries(aziotsharedutil
    mbedtls mbedcrypto mbedx509
    m
)

# Add sources
add_subdirectory( src )

# Add samples
add_subdirectory( samples )