set(APP_NAME "kvs_linux")

# compiled as C99
set(CMAKE_C_FLAGS "--std=c99 ${CMAKE_C_FLAGS}")

# compiled as c++11
set(CMAKE_CXX_FLAGS "--std=c++11 ${CMAKE_CXX_FLAGS}")

# support usleep
set(CMAKE_C_FLAGS "-D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=200112L ${CMAKE_C_FLAGS}")

set(${APP_NAME}_SRC
    ${APP_NAME}.c
)

if(${BUILD_SHARED_SAMPLES})
    add_executable(${APP_NAME} ${${APP_NAME}_SRC})
    set_target_properties(${APP_NAME} PROPERTIES OUTPUT_NAME ${APP_NAME})
    target_link_libraries(${APP_NAME}
        kvs-embedded-c-shared
        samplescommon-shared
        pthread
    )
endif()

if(${BUILD_STATIC_SAMPLES})
    add_executable(${APP_NAME}-static ${${APP_NAME}_SRC})
    set_target_properties(${APP_NAME}-static PROPERTIES OUTPUT_NAME ${APP_NAME}-static)
    target_link_libraries(${APP_NAME}-static
        kvs-embedded-c-static
        samplescommon-static
        pthread
    )
endif()
