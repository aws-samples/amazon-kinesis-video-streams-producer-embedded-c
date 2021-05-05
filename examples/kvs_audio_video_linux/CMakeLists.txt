set ( APP_NAME "kvs_audio_video_linux" )

option(BUILD_STATIC "Build static executable" Off)
option(EXAMPLE_CONFIG_FROM_ENV "configure example from environment" Off)

set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99" )

# Make warning as error
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror" )

if(${BUILD_STATIC})
    set( CMAKE_EXE_LINKER_FLAGS "-static" )
endif()

if(${EXAMPLE_CONFIG_FROM_ENV})
    set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DEXAMPLE_CONFIG_FROM_ENV" )
endif()

add_executable(
    ${APP_NAME}
        "${APP_NAME}.c"
        "${CMAKE_SOURCE_DIR}/src/port/kvs_port_linux.c"
)

target_link_libraries(
    ${APP_NAME}
    PRIVATE
        kvs-producer-rtos
        mbedtls
        mbedcrypto
        mbedx509
        corehttp
        parson
)

target_include_directories(
    ${APP_NAME}
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_SOURCE_DIR}/src/include
        ${CMAKE_SOURCE_DIR}/libraries/aws/coreHTTP/source/include
        ${CMAKE_SOURCE_DIR}/libraries/3rdparty/mbedtls/include
        ${CMAKE_SOURCE_DIR}/libraries/3rdparty/parson
)