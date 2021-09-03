set(APP_NAME "kvsappcli-raspberry-pi")

set(${APP_NAME}_SRC
    ${APP_NAME}.c
)

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

if(${BUILD_SHARED_SAMPLES})
    add_executable(${APP_NAME} ${${APP_NAME}_SRC})
    set_target_properties(${APP_NAME} PROPERTIES OUTPUT_NAME ${APP_NAME})
    target_compile_definitions(${APP_NAME} PUBLIC -DBUILD_EXECUTABLE_WITH_SHARED_LIBRARY)
    target_compile_definitions(${APP_NAME} PUBLIC ${COMPILE_FLAGS_FOR_POOL_ALLOCATOR})
    target_link_libraries(${APP_NAME}
        kvsapp-shared
        samplescommon-shared
        pthread
        v4l2capturer-shared
    )
endif()

if(${BUILD_STATIC_SAMPLES})
    add_executable(${APP_NAME}-static ${${APP_NAME}_SRC})
    set_target_properties(${APP_NAME}-static PROPERTIES OUTPUT_NAME ${APP_NAME}-static)
    target_compile_definitions(${APP_NAME}-static PUBLIC -DBUILD_EXECUTABLE_WITH_STATIC_LIBRARY)
    target_compile_definitions(${APP_NAME}-static PUBLIC ${COMPILE_FLAGS_FOR_POOL_ALLOCATOR})
    target_link_libraries(${APP_NAME}-static
        kvsapp-static
        samplescommon-static
        pthread
        v4l2capturer-static
        ${LINKER_FLAGS_FOR_MEM_WRAPPER}
    )
endif()
