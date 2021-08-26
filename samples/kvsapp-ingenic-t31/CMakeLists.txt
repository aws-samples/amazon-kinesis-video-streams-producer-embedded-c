set(APP_NAME "kvsappcli-ingenic-t31")

# support usleep
set(CMAKE_C_FLAGS "-D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=200112L ${CMAKE_C_FLAGS}")

# link flags
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -muclibc -Wl,-gc-sections")

# t31 library
set(INGENIC_T31_SRC
    sdk/samples/libimp-samples/sample-common.c
)

set(INGENIC_T31_INC
    sdk/samples/libimp-samples
    sdk/include
)

# setup shared library
add_library(ingenic-t31-shared SHARED ${INGENIC_T31_SRC})
set_target_properties(ingenic-t31-shared PROPERTIES POSITION_INDEPENDENT_CODE 1)
set_target_properties(ingenic-t31-shared PROPERTIES OUTPUT_NAME ingenic-t31)
target_include_directories(ingenic-t31-shared PUBLIC ${INGENIC_T31_INC})

# setup static library
add_library(ingenic-t31-static STATIC ${INGENIC_T31_SRC})
set_target_properties(ingenic-t31-static PROPERTIES OUTPUT_NAME ingenic-t31)
target_include_directories(ingenic-t31-static PUBLIC ${INGENIC_T31_INC})

# kvsappcli executable
set(${APP_NAME}_SRC
    source/kvsappcli.c
    source/t31_video.c
    source/t31_audio.c
)

set(${APP_NAME}_INC
    include
)

# build shared executable
add_executable(${APP_NAME} ${${APP_NAME}_SRC})
set_target_properties(${APP_NAME} PROPERTIES OUTPUT_NAME ${APP_NAME})
target_include_directories(${APP_NAME} PUBLIC ${${APP_NAME}_INC})
target_link_directories(${APP_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sdk/lib/uclibc)
target_link_libraries(${APP_NAME}
    rt
    dl
    ingenic-t31-shared
    aac-encoder-shared
    imp
    alog
    fdk-aac-shared
    aziotsharedutil-shared
    kvs-embedded-c-shared
    kvsapp-shared
    pthread
)

# build static executable
add_executable(${APP_NAME}-static ${${APP_NAME}_SRC})
set_target_properties(${APP_NAME}-static PROPERTIES OUTPUT_NAME ${APP_NAME}-static)
target_include_directories(${APP_NAME}-static PUBLIC ${${APP_NAME}_INC})
target_link_directories(${APP_NAME}-static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sdk/lib/uclibc)
target_link_libraries(${APP_NAME}-static
    rt
    dl
    ingenic-t31-static
    aac-encoder-static
    imp.a
    alog.a
    fdk-aac-static
    aziotsharedutil-static
    kvs-embedded-c-static
    kvsapp-static
    pthread
)
