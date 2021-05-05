cmake_minimum_required( VERSION 3.2.0 )

project(kvs-producer-rtos C)

# Make warning as error
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")

option(ENABLE_LOG_ERROR "Enable error log" On)
option(ENABLE_LOG_WARN "Enable error log" On)
option(ENABLE_LOG_INFO "Enable error log" On)
option(ENABLE_LOG_DEBUG "Enable error log" Off)

# options for debug log
if( NOT ${ENABLE_DEBUG_LOG})
	set(ENABLE_LOG_ERROR off)
	set(ENABLE_LOG_WARN off)
	set(ENABLE_LOG_INFO off)
	set(ENABLE_LOG_DEBUG off)
endif()

if(${ENABLE_LOG_ERROR})
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_LOG_ERROR")
endif()

if(${ENABLE_LOG_WARN})
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_LOG_WARN")
endif()

if(${ENABLE_LOG_INFO})
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_LOG_INFO")
endif()

if(${ENABLE_LOG_DEBUG})
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_LOG_DEBUG")
endif()

add_library(kvs-producer-rtos STATIC
	source/aws_signer_v4.c
	source/http_helper.c
	source/kvs_rest_api.c
	source/mkv_generator.c
	source/network_api.c
	source/iot_credential_provider.c
	source/json_helper.c
)

target_include_directories(kvs-producer-rtos
    PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${CMAKE_SOURCE_DIR}/libraries/3rdparty/mbedtls/include
	${CMAKE_SOURCE_DIR}/libraries/3rdparty/parson
	${CMAKE_SOURCE_DIR}/libraries/3rdparty/llhttp/include
)

add_dependencies(kvs-producer-rtos
	mbedtls
	mbedcrypto
	mbedx509
	parson
	llhttp
)