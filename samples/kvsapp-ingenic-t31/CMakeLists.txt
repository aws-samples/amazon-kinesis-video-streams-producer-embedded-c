set(APP_NAME "kvsappcli-ingentic-t31")

# support usleep
set(CMAKE_C_FLAGS "-D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=200112L ${CMAKE_C_FLAGS}")

# strip symboles for release build
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s")

# link flags
set(CMAKE_SHARED_LINKER_FLAGS "{CMAKE_SHARED_LINKER_FLAGS} -muclibc -Wl,-gc-sections")

# t31 library
add_library(t31
    sdk/samples/libimp-samples/sample-common.c
)

target_include_directories(t31 PUBLIC
    sdk/samples/libimp-samples
    sdk/include
)

# header path of this executable
include_directories(include)

link_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/lib/uclibc
)

add_executable(${APP_NAME}
    source/kvsappcli.c
    source/t31_video.c
    source/t31_audio.c
)

target_link_libraries(${APP_NAME}
    rt
    dl
    t31
    aac_encoder
    imp.a
    alog.a
    fdk-aac
    aziotsharedutil
    kvs-embedded-c
    kvsapp
    pthread
)

add_custom_command(TARGET ${APP_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${APP_NAME}> ${PROJECT_BINARY_DIR}/
)

