cmake_minimum_required(VERSION 3.5)

set(KVS_EMBEDDED_C_SRC "${CMAKE_CURRENT_LIST_DIR}/../../../../src")

set(COMPONENT_SRCS
    ${KVS_EMBEDDED_C_SRC}/source/app/kvsapp.c
    ${KVS_EMBEDDED_C_SRC}/source/codec/nalu.c
    ${KVS_EMBEDDED_C_SRC}/source/codec/sps_decode.c
    ${KVS_EMBEDDED_C_SRC}/source/codec/sps_decode.h
    ${KVS_EMBEDDED_C_SRC}/source/misc/json_helper.c
    ${KVS_EMBEDDED_C_SRC}/source/misc/json_helper.h
    ${KVS_EMBEDDED_C_SRC}/source/mkv/mkv_generator.c
    ${KVS_EMBEDDED_C_SRC}/source/net/http_helper.c
    ${KVS_EMBEDDED_C_SRC}/source/net/http_helper.h
    ${KVS_EMBEDDED_C_SRC}/source/net/netio.c
    ${KVS_EMBEDDED_C_SRC}/source/net/netio.h
    ${KVS_EMBEDDED_C_SRC}/source/os/allocator.c
    ${KVS_EMBEDDED_C_SRC}/source/os/allocator.h
    ${KVS_EMBEDDED_C_SRC}/source/os/endian.h
    ${KVS_EMBEDDED_C_SRC}/source/restful/iot/iot_credential_provider.c
    ${KVS_EMBEDDED_C_SRC}/source/restful/kvs/restapi_kvs.c
    ${KVS_EMBEDDED_C_SRC}/source/restful/aws_signer_v4.c
    ${KVS_EMBEDDED_C_SRC}/source/restful/aws_signer_v4.h
    ${KVS_EMBEDDED_C_SRC}/source/stream/stream.c
)

set(COMPONENT_SRCS ${COMPONENT_SRCS}
    ${KVS_EMBEDDED_C_SRC}/port/port_esp32.c
)

set(COMPONENT_ADD_INCLUDEDIRS
    ${KVS_EMBEDDED_C_SRC}/include
    ${KVS_EMBEDDED_C_SRC}/source
)

set(COMPONENT_SUBMODULES "kvs-embedded-c")

set(COMPONENT_PRIV_REQUIRES "llhttp parson mbedtls esp-tls c-utility")

register_component()

component_compile_options (
    -Os
)

