cmake_minimum_required(VERSION 3.2.0)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake;${CMAKE_MODULE_PATH}")
project(kvs-embedded-c)

# Options
option(BUILD_SHARED_SAMPLES             "Build shared samples"                              ON)
option(BUILD_STATIC_SAMPLES             "Build static samples"                              ON)
option(BUILD_AAC_ENCODER                "Build aac-encoder"                                 OFF)
option(BOARD_INGENIC_T31                "Build board Ingenic T31"                           OFF)
option(BOARD_RPI                        "Build board Raspberry Pi"                          OFF)
option(USE_POOL_ALLOCATOR_LIB           "Use pool allocator on KVS lib only"                OFF)
option(USE_POOL_ALLOCATOR_ALL           "Apply pool allocator on KVS lib and executable"    OFF)
option(SAMPLE_OPTIONS_FROM_ENV_VAR      "Sample reads options from environment variable"    ON)
option(BUILD_WEBRTC_SAMPLES             "Build a sample that kvs and web rtc share buffers" OFF)

set(USE_WEBRTC_MBEDTLS_LIB      OFF)

# Verify and resolve options
if(${BOARD_INGENIC_T31})
    set(BUILD_SHARED_SAMPLES    OFF)
    set(BUILD_STATIC_SAMPLES    OFF)
    set(BUILD_AAC_ENCODER       ON)
endif()
if(${USE_POOL_ALLOCATOR_ALL})
    set(USE_POOL_ALLOCATOR_LIB   ON)
endif()
if(${BUILD_WEBRTC_SAMPLES})
    set(USE_WEBRTC_MBEDTLS_LIB  ON)
endif()

if (NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s -Wl,--gc-sections")
endif()

# Checking platform properties
include(CheckIncludeFiles)
check_include_files(signal.h HAVE_SIGNAL_H)

# Print option values
message(STATUS "BUILD_SHARED_SAMPLES            = ${BUILD_SHARED_SAMPLES}")
message(STATUS "BUILD_STATIC_SAMPLES            = ${BUILD_STATIC_SAMPLES}")
message(STATUS "BUILD_AAC_ENCODER               = ${BUILD_AAC_ENCODER}")
message(STATUS "BOARD_INGENIC_T31               = ${BOARD_INGENIC_T31}")
message(STATUS "USE_POOL_ALLOCATOR_LIB          = ${USE_POOL_ALLOCATOR_LIB}")
message(STATUS "USE_POOL_ALLOCATOR_ALL          = ${USE_POOL_ALLOCATOR_ALL}")
message(STATUS "SAMPLE_OPTIONS_FROM_ENV_VAR     = ${SAMPLE_OPTIONS_FROM_ENV_VAR}")
message(STATUS "CMAKE_BUILD_TYPE                = ${CMAKE_BUILD_TYPE}")
message(STATUS "BUILD_WEBRTC_SAMPLES            = ${BUILD_WEBRTC_SAMPLES}")

if(${BUILD_WEBRTC_SAMPLES})
    set(WEBRTC_LIB_PATH ${CMAKE_CURRENT_LIST_DIR}/libraries/amazon/amazon-kinesis-video-streams-webrtc-sdk-c/webrtc/open-source/lib)
    set(WEBRTC_INC_PATH
        ${CMAKE_CURRENT_LIST_DIR}/libraries/amazon/amazon-kinesis-video-streams-webrtc-sdk-c/webrtc/open-source/include
        ${CMAKE_CURRENT_LIST_DIR}/libraries/amazon/amazon-kinesis-video-streams-webrtc-sdk-c/webrtc/src/include
        ${CMAKE_CURRENT_LIST_DIR}/libraries/amazon/amazon-kinesis-video-streams-webrtc-sdk-c/webrtc/src/source
    )
    set(CMAKE_C_FLAGS "-I${CMAKE_CURRENT_SOURCE_DIR}/libraries/amazon/amazon-kinesis-video-streams-webrtc-sdk-c/webrtc/configs -DMBEDTLS_USER_CONFIG_FILE='<config_mbedtls.h>' ${CMAKE_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "-I${CMAKE_CURRENT_SOURCE_DIR}/libraries/amazon/amazon-kinesis-video-streams-webrtc-sdk-c/webrtc/configs -DMBEDTLS_USER_CONFIG_FILE='<config_mbedtls.h>' ${CMAKE_CXX_FLAGS}")

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-implicit-function-declaration -Wno-error=format -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -fgnu89-inline")
    set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -Wno-implicit-function-declaration -Wno-error=format -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -fgnu89-inline")

    add_definitions(-DSUPPORT_H264_AVCC_FRAME)
    add_definitions(-DSUPPORT_SHARE_BUFFER)
endif()

if(${BUILD_WEBRTC_SAMPLES})
    include(libwebrtc)
endif()

# Make warning as error
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")

# compiled as C99
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --std=c99")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11")

# Compile flags for Ingenic T31
if(${BOARD_INGENIC_T31})
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -Wall -march=mips32r2 -muclibc")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-missing-braces -Wno-strict-aliasing -Wno-unused-variable -Wno-unused-but-set-variable -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -Wno-unused-function")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -march=mips32r2 -muclibc")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-missing-braces -Wno-strict-aliasing -Wno-unused-variable -Wno-unused-but-set-variable -Wno-int-to-pointer-cast -Wno-maybe-uninitialized -Wno-unused-function")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -muclibc")
endif()

if(${USE_POOL_ALLOCATOR_LIB})
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--wrap,kvsMalloc -Wl,--wrap,kvsRealloc -Wl,--wrap,kvsCalloc -Wl,--wrap,kvsFree")
endif()

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

# Add thirdparty libraries
if(NOT ${USE_WEBRTC_MBEDTLS_LIB})
    include(libmbedtls)
endif()
include(libllhttp)
include(libparson)
include(libaziotsharedutil)
include(libtlsf)
if(${BUILD_AAC_ENCODER})
    include(libfdkaac)
endif()
if(${BOARD_RPI})
   include(libv4l2capturer)
endif()

# Add sources
add_subdirectory(src)

# Add application
add_subdirectory(app)

# Add samples
add_subdirectory(samples)
