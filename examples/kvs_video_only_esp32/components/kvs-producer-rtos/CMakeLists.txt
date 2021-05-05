cmake_minimum_required(VERSION 3.5)

set(KVS_PRODUCER_RTOS_SRC                   "${CMAKE_CURRENT_LIST_DIR}/../../../../src")

set(COMPONENT_SRCS
        ${KVS_PRODUCER_RTOS_SRC}/source/aws_signer_v4.c
        ${KVS_PRODUCER_RTOS_SRC}/source/http_helper.c
        ${KVS_PRODUCER_RTOS_SRC}/source/iot_credential_provider.c
        ${KVS_PRODUCER_RTOS_SRC}/source/json_helper.c
        ${KVS_PRODUCER_RTOS_SRC}/source/kvs_rest_api.c
        ${KVS_PRODUCER_RTOS_SRC}/source/mkv_generator.c
        ${KVS_PRODUCER_RTOS_SRC}/source/network_api.c
        ${KVS_PRODUCER_RTOS_SRC}/port/kvs_port_esp32.c
        )

set (COMPONENT_ADD_INCLUDEDIRS
        ${KVS_PRODUCER_RTOS_SRC}/include
        )

set(COMPONENT_SUBMODULES "kvs-producer-rtos")

set(COMPONENT_PRIV_REQUIRES "llhttp parson mbedtls esp-tls")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_LOG_ERROR")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_LOG_WARN")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_LOG_INFO")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_LOG_DEBUG")

register_component()

component_compile_options (
        -Wno-stringop-truncation
        -Os
)
