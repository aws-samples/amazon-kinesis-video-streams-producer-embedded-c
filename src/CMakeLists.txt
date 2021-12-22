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
    ${LIB_DIR}/include/kvs/errors.h
    ${LIB_DIR}/include/kvs/iot_credential_provider.h
    ${LIB_DIR}/include/kvs/mkv_generator.h
    ${LIB_DIR}/include/kvs/mkv_parser.h
    ${LIB_DIR}/include/kvs/nalu.h
    ${LIB_DIR}/include/kvs/pool_allocator.h
    ${LIB_DIR}/include/kvs/port.h
    ${LIB_DIR}/include/kvs/restapi.h
    ${LIB_DIR}/include/kvs/stream.h
    ${LIB_DIR}/source/allocator.c
    ${LIB_DIR}/source/allocator.h
    ${LIB_DIR}/source/aws_signer_v4.c
    ${LIB_DIR}/source/aws_signer_v4.h
    ${LIB_DIR}/source/endian.h
    ${LIB_DIR}/source/http_helper.c
    ${LIB_DIR}/source/http_helper.h
    ${LIB_DIR}/source/iot_credential_provider.c
    ${LIB_DIR}/source/json_helper.c
    ${LIB_DIR}/source/json_helper.h
    ${LIB_DIR}/source/mkv_generator.c
    ${LIB_DIR}/source/mkv_parser.c
    ${LIB_DIR}/source/nalu.c
    ${LIB_DIR}/source/netio.c
    ${LIB_DIR}/source/netio.h
    ${LIB_DIR}/source/pool_allocator.c
    ${LIB_DIR}/source/restapi.c
    ${LIB_DIR}/source/sps_decode.c
    ${LIB_DIR}/source/sps_decode.h
    ${LIB_DIR}/source/stream.c
)

set(LIB_INC
    ${LIB_DIR}/include
)

set(LINK_INTERFACE_LIBS
    llhttp
    parson
    aziotsharedutil
    tlsf
)
if(NOT ${USE_WEBRTC_MBEDTLS_LIB})
    set(LINK_INTERFACE_LIBS ${LINK_INTERFACE_LIBS}
        mbedtls
        mbedcrypto
        mbedx509
    )
endif()

set(LINK_SHARED_LIBS
    llhttp-shared
    parson-shared
    aziotsharedutil-shared
    tlsf-shared
)
if(NOT ${USE_WEBRTC_MBEDTLS_LIB})
    set(LINK_SHARED_LIBS ${LINK_SHARED_LIBS}
        mbedtls-shared
        mbedcrypto-shared
        mbedx509-shared
    )
endif()

set(LINK_STATIC_LIBS
    llhttp-static
    parson-static
    aziotsharedutil-static
    tlsf-static
)
if(NOT ${USE_WEBRTC_MBEDTLS_LIB})
    set(LINK_STATIC_LIBS ${LINK_STATIC_LIBS}
        mbedtls-static
        mbedcrypto-static
        mbedx509-static
    )
endif()

if(UNIX)
    set(LIB_SRC ${LIB_SRC}
        ${LIB_DIR}/port/port_linux.c
    )
endif()

# setup static library
add_library(${LIB_NAME} STATIC ${LIB_SRC})
set_target_properties(${LIB_NAME} PROPERTIES POSITION_INDEPENDENT_CODE 1)
target_include_directories(${LIB_NAME} PUBLIC ${LIB_INC})
if(${USE_WEBRTC_MBEDTLS_LIB})
    target_link_directories(${LIB_NAME} PUBLIC ${WEBRTC_LIB_PATH})
    target_include_directories(${LIB_NAME} PUBLIC ${WEBRTC_INC_PATH})
endif()
target_link_libraries(${LIB_NAME} PUBLIC
    ${LINK_INTERFACE_LIBS}
)

# kvs-embedded-c with shared libraries
add_library(${LIB_NAME}-shared INTERFACE)
target_link_libraries(${LIB_NAME}-shared INTERFACE
    ${LIB_NAME}
    ${LINK_SHARED_LIBS}
)

# kvs-embedded-c with static libraries
add_library(${LIB_NAME}-static INTERFACE)
target_link_libraries(${LIB_NAME}-static INTERFACE
    ${LIB_NAME}
    ${LINK_STATIC_LIBS}
)

include(GNUInstallDirs)

install(TARGETS ${LIB_NAME}
        LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
)

install(TARGETS ${LIB_NAME}-shared
        LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
)

install(TARGETS ${LIB_NAME}-static
        LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
)

install(DIRECTORY ${LIB_INC}/
        DESTINATION ${CMAKE_INSTALL_FULL_INCLUDEDIR}
)
