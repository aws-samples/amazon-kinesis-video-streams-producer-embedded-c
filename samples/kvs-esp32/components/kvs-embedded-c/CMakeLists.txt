cmake_minimum_required(VERSION 3.5)

set(KVS_EMBEDDED_C_SRC "${CMAKE_CURRENT_LIST_DIR}/../../../../src")

set(COMPONENT_SRCS
    ${KVS_EMBEDDED_C_SRC}/source/aws_signer_v4.c
    ${KVS_EMBEDDED_C_SRC}/source/http_helper.c
    ${KVS_EMBEDDED_C_SRC}/source/iot_credential_provider.c
    ${KVS_EMBEDDED_C_SRC}/source/json_helper.c
    ${KVS_EMBEDDED_C_SRC}/source/mkv_generator.c
    ${KVS_EMBEDDED_C_SRC}/source/nalu.c
    ${KVS_EMBEDDED_C_SRC}/source/netio.c
    ${KVS_EMBEDDED_C_SRC}/source/restapi.c
    ${KVS_EMBEDDED_C_SRC}/source/sps_decode.c
    ${KVS_EMBEDDED_C_SRC}/source/stream.c
)

set(COMPONENT_SRCS ${COMPONENT_SRCS}
    ${KVS_EMBEDDED_C_SRC}/port/port_esp32.c
)

set(COMPONENT_ADD_INCLUDEDIRS
    ${KVS_EMBEDDED_C_SRC}/include
)

set(COMPONENT_SUBMODULES "kvs-embedded-c")

set(COMPONENT_PRIV_REQUIRES "llhttp parson mbedtls esp-tls c-utility")

register_component()

component_compile_options (
    -Os
)

