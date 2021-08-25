cmake_minimum_required(VERSION 3.2.0)
project(kvs-embedded-c)

# Options
option(BUILD_SAMPLES "Build samples" ON)
option(BOARD "The target board" "OFF")
option(USE_POOL_ALLOCATOR "Use pool allocator" OFF)

# Make warning as error
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")

# compiled as C99
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --std=c99")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11")

# Add thirdparty library tlsf
if(USE_POOL_ALLOCATOR)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,--wrap,malloc -Wl,--wrap,free -Wl,--wrap,realloc -Wl,--wrap,calloc")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--wrap,malloc -Wl,--wrap,free -Wl,--wrap,realloc -Wl,--wrap,calloc")
    add_definitions(-DKVS_USE_POOL_ALLOCATOR)
    set(TLSF_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libraries/3rdparty/tlsf)
    add_library(tlsf
        ${TLSF_DIR}/tlsf.c
        ${TLSF_DIR}/tlsf.h
    )
    target_include_directories(tlsf PUBLIC
        ${TLSF_DIR}
    )
endif()

if("${BOARD}" STREQUAL "INGENTIC_T31")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Os -Wall -march=mips32r2 -muclibc")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-missing-braces -Wno-strict-aliasing -Wno-unused-variable -Wno-unused-but-set-variable -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -Wno-unused-function")

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os -Wall -march=mips32r2 -muclibc")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-missing-braces -Wno-strict-aliasing -Wno-unused-variable -Wno-unused-but-set-variable -Wno-int-to-pointer-cast -Wno-maybe-uninitialized -Wno-unused-function")
endif()

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

# Add FDK AAC for Ingentic T31
if("${BOARD}" STREQUAL "INGENTIC_T31")
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "")
    add_subdirectory(libraries/3rdparty/fdk-aac)
endif()

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
add_subdirectory(src)

# Add application
add_subdirectory(app)

if(BUILD_SAMPLES)
    # Add samples
    add_subdirectory(samples)
endif()
