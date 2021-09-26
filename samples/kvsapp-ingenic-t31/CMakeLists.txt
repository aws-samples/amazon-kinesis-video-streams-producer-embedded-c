set(APP_NAME "kvsappcli-ingenic-t31")

# t31 library
set(INGENIC_T31_SRC
    sdk/samples/libimp-samples/sample-common.c
)

set(INGENIC_T31_INC
    sdk/samples/libimp-samples
    sdk/include
)

# setup static library
add_library(ingenic-t31 STATIC ${INGENIC_T31_SRC})
target_compile_definitions(ingenic-t31 PUBLIC -D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=200112L)
set_target_properties(ingenic-t31 PROPERTIES OUTPUT_NAME ingenic-t31)
target_include_directories(ingenic-t31 PUBLIC ${INGENIC_T31_INC})

# kvsappcli executable
set(${APP_NAME}_SRC
    source/kvsappcli.c
    source/t31_video.c
    source/t31_audio.c
)

set(${APP_NAME}_INC
    include
)

unset(COMPILE_FLAGS_FOR_POOL_ALLOCATOR)
if(${USE_POOL_ALLOCATOR_LIB})
    set(COMPILE_FLAGS_FOR_POOL_ALLOCATOR -DKVS_USE_POOL_ALLOCATOR)
endif()

unset(LINKER_FLAGS_FOR_MEM_WRAPPER)
if(${USE_POOL_ALLOCATOR_ALL})
    set(${APP_NAME}_SRC ${${APP_NAME}_SRC}
        source/mem_wrapper.c
    )
    set(LINKER_FLAGS_FOR_MEM_WRAPPER -Wl,--wrap,malloc -Wl,--wrap,realloc -Wl,--wrap,calloc -Wl,--wrap,free)
endif()

# build shared executable
add_executable(${APP_NAME} ${${APP_NAME}_SRC})
set_target_properties(${APP_NAME} PROPERTIES OUTPUT_NAME ${APP_NAME})
# support usleep
target_compile_definitions(${APP_NAME} PUBLIC -D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=200112L)
target_compile_definitions(${APP_NAME} PUBLIC -DBUILD_EXECUTABLE_WITH_SHARED_LIBRARY)
target_compile_definitions(${APP_NAME} PUBLIC ${COMPILE_FLAGS_FOR_POOL_ALLOCATOR})
target_include_directories(${APP_NAME} PUBLIC ${${APP_NAME}_INC})
target_link_directories(${APP_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sdk/lib/uclibc)
target_link_libraries(${APP_NAME}
    rt
    dl
    ingenic-t31
    aac-encoder-shared
    imp
    alog
    pthread
    kvsapp-shared
)

# build static executable
add_executable(${APP_NAME}-static ${${APP_NAME}_SRC})
set_target_properties(${APP_NAME}-static PROPERTIES OUTPUT_NAME ${APP_NAME}-static)
# support usleep
target_compile_definitions(${APP_NAME}-static PUBLIC -D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=200112L)
target_compile_definitions(${APP_NAME}-static PUBLIC -DBUILD_EXECUTABLE_WITH_STATIC_LIBRARY)
target_compile_definitions(${APP_NAME}-static PUBLIC ${COMPILE_FLAGS_FOR_POOL_ALLOCATOR})
target_include_directories(${APP_NAME}-static PUBLIC ${${APP_NAME}_INC})
target_link_directories(${APP_NAME}-static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sdk/lib/uclibc)
target_link_libraries(${APP_NAME}-static
    rt
    dl
    ingenic-t31
    aac-encoder-static
    imp.a
    alog.a
    pthread
    kvsapp-static
    ${LINKER_FLAGS_FOR_MEM_WRAPPER}
)

# create sample patch
set(INGENIC_T31_KVS_SAMPLE_DIR ${PROJECT_BINARY_DIR}/ingenic_t31_kvs_sample)

add_custom_command(TARGET ${APP_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -rf         ${INGENIC_T31_KVS_SAMPLE_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${INGENIC_T31_KVS_SAMPLE_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy           ${CMAKE_CURRENT_SOURCE_DIR}/res/CMakeLists.txt ${INGENIC_T31_KVS_SAMPLE_DIR}/CMakeLists.txt
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBRARY_OUTPUT_PATH} ${INGENIC_T31_KVS_SAMPLE_DIR}/lib
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/sdk/lib/uclibc ${INGENIC_T31_KVS_SAMPLE_DIR}/uclibc
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include/imp ${INGENIC_T31_KVS_SAMPLE_DIR}/include/imp
    COMMAND ${CMAKE_COMMAND} -E copy           ${CMAKE_CURRENT_SOURCE_DIR}/sdk/samples/libimp-samples/sample-common.h ${INGENIC_T31_KVS_SAMPLE_DIR}/include/sample-common.h
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/source ${INGENIC_T31_KVS_SAMPLE_DIR}/source
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/include ${INGENIC_T31_KVS_SAMPLE_DIR}/include
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/app/aac-encoder/include ${INGENIC_T31_KVS_SAMPLE_DIR}/include
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/app/kvsapp/include ${INGENIC_T31_KVS_SAMPLE_DIR}/include
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/src/include ${INGENIC_T31_KVS_SAMPLE_DIR}/include
)
