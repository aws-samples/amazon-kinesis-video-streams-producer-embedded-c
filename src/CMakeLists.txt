cmake_minimum_required(VERSION 3.2.0)

set(LIB_NAME "kvs-embedded-c")

# compiled as C99
set(CMAKE_C_FLAGS "--std=c99 ${CMAKE_C_FLAGS}")

# compiled as c++11
set(CMAKE_CXX_FLAGS "--std=c++11 ${CMAKE_CXX_FLAGS}")

# needed for gettimeofday()
set(CMAKE_C_FLAGS "-D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=200112L ${CMAKE_C_FLAGS}")

set(LIB_SRC
    source/aws_signer_v4.c
    source/http_helper.c
    source/iot_credential_provider.c
    source/json_helper.c
    source/mkv_generator.c
    source/mkv_parser.c
    source/nalu.c
    source/netio.c
    source/restapi.c
    source/sps_decode.c
    source/stream.c
)

set(LINK_LIBS
    mbedtls
    mbedcrypto
    mbedx509
    llhttp
    parson
    aziotsharedutil)

if(USE_POOL_ALLOCATOR)
    set(LIB_SRC ${LIB_SRC}
        source/pool_allocator.c
    )
    set(LINK_LIBS ${LINK_LIBS}
        tlsf
    )
endif()

if(UNIX)
    set(LIB_SRC ${LIB_SRC}
        port/port_linux.c
    )
endif()

add_library(${LIB_NAME} STATIC
    ${LIB_SRC}
)

target_include_directories(${LIB_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${LIB_NAME} ${LINK_LIBS})
