# compiled as C99
set(CMAKE_C_FLAGS "--std=c99 ${CMAKE_C_FLAGS}")

# support usleep
set(CMAKE_C_FLAGS "-D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=200112L ${CMAKE_C_FLAGS}")

add_library(kvsapp
    source/kvsapp.c
)

target_include_directories(kvsapp PUBLIC
    include
)

target_link_libraries(kvsapp
    kvs-embedded-c
)