set(APP_NAME "kvs_with_webrtc")

set(${APP_NAME}_SRC
    kvs_with_webrtc.c
    kvs_with_webrtc.h
    # KVS producer source and headers
    kvsappcli.c
    sample_config.h
    option_configuration.c
    option_configuration.h
    # WebRTC source and headers
    kvsWebRTCClientMaster.c
    Common.c
    Samples.h
)

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
        mem_wrapper.c
    )
    set(LINKER_FLAGS_FOR_MEM_WRAPPER -Wl,--wrap,malloc -Wl,--wrap,realloc -Wl,--wrap,calloc -Wl,--wrap,free)
endif()

add_executable(${APP_NAME} ${${APP_NAME}_SRC})
set_target_properties(${APP_NAME} PROPERTIES OUTPUT_NAME ${APP_NAME})
# support usleep
#target_compile_definitions(${APP_NAME} PUBLIC -D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=200112L)
target_compile_definitions(${APP_NAME} PUBLIC -D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=200809L)
target_compile_definitions(${APP_NAME} PUBLIC -DBUILD_EXECUTABLE_WITH_STATIC_LIBRARY)
target_compile_definitions(${APP_NAME} PUBLIC ${COMPILE_FLAGS_FOR_POOL_ALLOCATOR})
target_compile_definitions(${APP_NAME} PUBLIC ${COMPILE_FLAGS_FOR_SAMPLE_OPTIONS_FROM_ENV_VAR})
target_compile_definitions(${APP_NAME} PUBLIC -DKVS_WITH_WEBRTC)
target_link_directories(${APP_NAME} PUBLIC ${WEBRTC_LIB_PATH})
target_include_directories(${APP_NAME} PUBLIC ${WEBRTC_INC_PATH})
target_link_libraries(${APP_NAME}
    kvsWebrtcClient
    kvsWebrtcSignalingClient
    kvsCommonLws
    kvspicUtils
    websockets
    kvs-embedded-c
    frame-ring-buffer
    ${LINKER_FLAGS_FOR_MEM_WRAPPER}
)

