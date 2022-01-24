cmake_minimum_required(VERSION 3.2.0)
project(kvs-ingenic-t31)

set(APP_NAME "kvsappcli-ingenic-t31")

# Options
option(USE_POOL_ALLOCATOR_LIB           "Use pool allocator on KVS lib only"                OFF)
option(USE_POOL_ALLOCATOR_ALL           "Apply pool allocator on KVS lib and executable"    OFF)
option(SAMPLE_OPTIONS_FROM_ENV_VAR      "Sample reads options from environment variable"    ON)

# Verify and resolve options
if(${USE_POOL_ALLOCATOR_ALL})
    set(USE_POOL_ALLOCATOR_LIB   ON)
endif()

# Print option values
message(STATUS "USE_POOL_ALLOCATOR_LIB          = ${USE_POOL_ALLOCATOR_LIB}")
message(STATUS "USE_POOL_ALLOCATOR_ALL          = ${USE_POOL_ALLOCATOR_ALL}")

# Make warning as error
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")

# compiled as C99
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --std=c99")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11")

# Compile flags for Ingenic T31
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -Wall -march=mips32r2 -muclibc")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-missing-braces -Wno-strict-aliasing -Wno-unused-variable -Wno-unused-but-set-variable -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -Wno-unused-function")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -march=mips32r2 -muclibc")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-missing-braces -Wno-strict-aliasing -Wno-unused-variable -Wno-unused-but-set-variable -Wno-int-to-pointer-cast -Wno-maybe-uninitialized -Wno-unused-function")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -muclibc -Wl,--gc-sections -s")

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

unset(COMPILE_FLAGS_FOR_SAMPLE_OPTIONS_FROM_ENV_VAR)
if(${SAMPLE_OPTIONS_FROM_ENV_VAR})
    set(COMPILE_FLAGS_FOR_SAMPLE_OPTIONS_FROM_ENV_VAR -DSAMPLE_OPTIONS_FROM_ENV_VAR)
endif()

unset(COMPILE_FLAGS_FOR_POOL_ALLOCATOR)
if(${USE_POOL_ALLOCATOR_LIB})
    set(COMPILE_FLAGS_FOR_POOL_ALLOCATOR -DKVS_USE_POOL_ALLOCATOR)
endif()

unset(LINKER_FLAGS_FOR_MEM_WRAPPER)
if(${USE_POOL_ALLOCATOR_ALL})
    set(${APP_NAME}_SRC ${${APP_NAME}_SRC}
        source/mem_wrapper.c
    )
    set(LINKER_FLAGS_FOR_MEM_WRAPPER -Wl,--wrap,malloc -Wl,--wrap,realloc -Wl,--wrap,calloc -Wl,--wrap,free)
endif()

# kvsappcli executable
set(${APP_NAME}_SRC
    include/aac_encoder.h
    include/alaw_encoder.h
    include/option_configuration.h
    include/sample_config.h
    include/t31_audio.h
    include/t31_video.h
    source/aac_encoder.c
    source/alaw_encoder.c
    source/kvsappcli.c
    source/option_configuration.c
    source/t31_audio.c
    source/t31_video.c
)

set(${APP_NAME}_INC
    include
)

unset(COMPILE_FLAGS_FOR_POOL_ALLOCATOR)
if(${USE_POOL_ALLOCATOR_LIB})
    set(COMPILE_FLAGS_FOR_POOL_ALLOCATOR -DKVS_USE_POOL_ALLOCATOR)
endif()

# build static executable
add_executable(${APP_NAME} ${${APP_NAME}_SRC})
target_compile_definitions(${APP_NAME} PUBLIC -D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=200112L)
target_compile_definitions(${APP_NAME} PUBLIC ${COMPILE_FLAGS_FOR_POOL_ALLOCATOR})
target_compile_definitions(${APP_NAME} PUBLIC ${COMPILE_FLAGS_FOR_SAMPLE_OPTIONS_FROM_ENV_VAR})
target_include_directories(${APP_NAME} PUBLIC ${${APP_NAME}_INC})
target_link_directories(${APP_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/uclibc
)
target_link_libraries(${APP_NAME}
    rt
    dl
    ingenic-t31
    imp.a
    alog.a
    pthread
    kvs-embedded-c
    mbedtls
    mbedcrypto
    mbedx509
    llhttp
    parson
    tlsf
    aziotsharedutil
    m
    ${LINKER_FLAGS_FOR_MEM_WRAPPER}
)