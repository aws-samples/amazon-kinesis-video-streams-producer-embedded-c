# compiled as C99
set(CMAKE_C_FLAGS "--std=c99 ${CMAKE_C_FLAGS}")

# support usleep
set(CMAKE_C_FLAGS "-D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=200112L ${CMAKE_C_FLAGS}")

set(KVSAPP_SRC
    source/kvsapp.c
)

set(KVSAPP_INC
    include
)

# setup shared library
add_library(kvsapp-shared SHARED ${KVSAPP_SRC})
set_target_properties(kvsapp-shared PROPERTIES OUTPUT_NAME kvsapp)
set_target_properties(kvsapp-shared PROPERTIES POSITION_INDEPENDENT_CODE 1)
target_include_directories(kvsapp-shared PUBLIC ${KVSAPP_INC})
target_link_libraries(kvsapp-shared PUBLIC
    kvs-embedded-c-shared aziotsharedutil-shared
)

# setup static library
add_library(kvsapp-static STATIC ${KVSAPP_SRC})
set_target_properties(kvsapp-static PROPERTIES OUTPUT_NAME kvsapp)
target_include_directories(kvsapp-static PUBLIC ${KVSAPP_INC})
target_link_libraries(kvsapp-static PUBLIC
    kvs-embedded-c-static aziotsharedutil-static
)