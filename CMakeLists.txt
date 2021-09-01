cmake_minimum_required(VERSION 3.2.0)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake;${CMAKE_MODULE_PATH}")
project(kvs-embedded-c)

# Options
option(BUILD_SHARED_SAMPLES             "Build shared samples"                              ON)
option(BUILD_STATIC_SAMPLES             "Build static samples"                              ON)
option(BUILD_AAC_ENCODER                "Build aac-encoder"                                 OFF)
option(BOARD_INGENTIC_T31               "Build board Ingentic T31"                          OFF)
option(USE_POOL_ALLOCATOR_LIB           "Use pool allocator on KVS lib only"                OFF)
option(USE_POOL_ALLOCATOR_ALL           "Apply pool allocator on KVS lib and executable"    OFF)

# Verify and resolve options
if(${BOARD_INGENTIC_T31})
    set(BUILD_SHARED_SAMPLES    OFF)
    set(BUILD_STATIC_SAMPLES    OFF)
    set(BUILD_AAC_ENCODER       ON)
endif()
if(${USE_POOL_ALLOCATOR_ALL})
    set(USE_POOL_ALLOCATOR_LIB   ON)
endif()

# Print option values
message(STATUS "BUILD_SHARED_SAMPLES            = ${BUILD_SHARED_SAMPLES}")
message(STATUS "BUILD_STATIC_SAMPLES            = ${BUILD_STATIC_SAMPLES}")
message(STATUS "BUILD_AAC_ENCODER               = ${BUILD_AAC_ENCODER}")
message(STATUS "BOARD_INGENTIC_T31              = ${BOARD_INGENTIC_T31}")
message(STATUS "USE_POOL_ALLOCATOR_LIB          = ${USE_POOL_ALLOCATOR_LIB}")
message(STATUS "USE_POOL_ALLOCATOR_ALL          = ${USE_POOL_ALLOCATOR_ALL}")

# Make warning as error
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")

# compiled as C99
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --std=c99")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11")

# Compile flags for Ingentic T31
if(${BOARD_INGENTIC_T31})
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -Wall -march=mips32r2 -muclibc")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-missing-braces -Wno-strict-aliasing -Wno-unused-variable -Wno-unused-but-set-variable -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -Wno-unused-function")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -march=mips32r2 -muclibc")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-missing-braces -Wno-strict-aliasing -Wno-unused-variable -Wno-unused-but-set-variable -Wno-int-to-pointer-cast -Wno-maybe-uninitialized -Wno-unused-function")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -muclibc")
endif()

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")
if(${USE_POOL_ALLOCATOR_LIB})
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--wrap,kvsMalloc -Wl,--wrap,kvsRealloc -Wl,--wrap,kvsCalloc -Wl,--wrap,kvsFree")
endif()

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

# Add thirdparty libraries
include(libmbedtls)
include(libllhttp)
include(libparson)
include(libaziotsharedutil)
include(libtlsf)
if(${BUILD_AAC_ENCODER})
    include(libfdkaac)
endif()

# Add sources
add_subdirectory(src)

# Add application
add_subdirectory(app)

# Add samples
add_subdirectory(samples)
