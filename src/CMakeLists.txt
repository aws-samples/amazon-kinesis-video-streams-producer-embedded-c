cmake_minimum_required(VERSION 3.2.0)

set(LIB_NAME "kvs-embedded-c")

# compiled as C99
set(CMAKE_C_FLAGS "--std=c99 ${CMAKE_C_FLAGS}")

# compiled as c++11
set(CMAKE_CXX_FLAGS "--std=c++11 ${CMAKE_CXX_FLAGS}")

# needed for gettimeofday()
set(CMAKE_C_FLAGS "-D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=200112L ${CMAKE_C_FLAGS}")

set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(LIB_SRC
    ${LIB_DIR}/source/aws_signer_v4.c
    ${LIB_DIR}/source/http_helper.c
    ${LIB_DIR}/source/iot_credential_provider.c
    ${LIB_DIR}/source/json_helper.c
    ${LIB_DIR}/source/mkv_generator.c
    ${LIB_DIR}/source/mkv_parser.c
    ${LIB_DIR}/source/nalu.c
    ${LIB_DIR}/source/netio.c
    ${LIB_DIR}/source/restapi.c
    ${LIB_DIR}/source/sps_decode.c
    ${LIB_DIR}/source/stream.c
)

set(LIB_INC
    ${LIB_DIR}//include
)

set(LINK_STATIC_LIBS
    mbedtls_static
    mbedcrypto_static
    mbedx509_static
    llhttp-static
    parson-static
    aziotsharedutil-static
)

set(LINK_SHARED_LIBS
    mbedtls
    mbedcrypto
    mbedx509
    llhttp-shared
    parson-shared
    aziotsharedutil-shared
)

if(USE_POOL_ALLOCATOR)
    set(LIB_SRC ${LIB_SRC}
        ${LIB_DIR}/source/pool_allocator.c
    )
    set(LINK_STATIC_LIBS ${LINK_STATIC_LIBS}
        tlsf-static
    )
    set(LINK_SHARED_LIBS ${LINK_SHARED_LIBS}
        tlsf-shared
    )
endif()

if(UNIX)
    set(LIB_SRC ${LIB_SRC}
        ${LIB_DIR}/port/port_linux.c
    )
endif()

# setup shared library
add_library(${LIB_NAME}-shared SHARED ${LIB_SRC})
set_target_properties(${LIB_NAME}-shared PROPERTIES POSITION_INDEPENDENT_CODE 1)
set_target_properties(${LIB_NAME}-shared PROPERTIES OUTPUT_NAME ${LIB_NAME})
target_include_directories(${LIB_NAME}-shared PUBLIC ${LIB_INC})
target_link_libraries(${LIB_NAME}-shared PUBLIC
    ${LINK_SHARED_LIBS}
)

# setup static library
add_library(${LIB_NAME}-static STATIC ${LIB_SRC})
set_target_properties(${LIB_NAME}-static PROPERTIES OUTPUT_NAME ${LIB_NAME})
target_include_directories(${LIB_NAME}-static PUBLIC ${LIB_INC})
target_link_libraries(${LIB_NAME}-static PUBLIC
    ${LINK_STATIC_LIBS}
)
