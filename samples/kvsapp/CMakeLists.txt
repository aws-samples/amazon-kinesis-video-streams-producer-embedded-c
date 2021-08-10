set(APP_NAME "kvsappcli")

include_directories(.)

# compiled as C99
set(CMAKE_C_FLAGS "--std=c99 ${CMAKE_C_FLAGS}")

# support usleep
set(CMAKE_C_FLAGS "-D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=200112L ${CMAKE_C_FLAGS}")

add_executable(${APP_NAME}
    ${APP_NAME}.c
)

target_link_libraries(${APP_NAME}
    kvs-embedded-c kvsapp samplescommon pthread
)

add_custom_command(TARGET ${APP_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${APP_NAME}> ${PROJECT_BINARY_DIR}/
)