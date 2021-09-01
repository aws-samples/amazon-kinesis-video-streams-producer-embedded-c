set(KVSAPP_SRC
    source/kvsapp.c
)

set(KVSAPP_INC
    include
)

# setup static library
add_library(kvsapp STATIC ${KVSAPP_SRC})
target_include_directories(kvsapp PUBLIC ${KVSAPP_INC})
target_compile_options(kvsapp PUBLIC --std=c99)
target_compile_definitions(kvsapp PUBLIC -D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=200112L)
target_link_libraries(kvsapp PUBLIC
    kvs-embedded-c
    aziotsharedutil
)

# kvsapp with shared libraries
add_library(kvsapp-shared INTERFACE)
target_link_libraries(kvsapp-shared INTERFACE
    kvsapp
    kvs-embedded-c-shared
    aziotsharedutil-shared
)

# kvs-embedded-c with static libraries
add_library(kvsapp-static INTERFACE)
target_link_libraries(kvsapp-static INTERFACE
    kvsapp
    kvs-embedded-c-static
    aziotsharedutil-static
)