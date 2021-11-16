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

include(GNUInstallDirs)

install(TARGETS kvsapp
        LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
)

install(TARGETS kvsapp-shared
        LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
)

install(TARGETS kvsapp-static
        LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
)

install(DIRECTORY ${KVSAPP_INC}/
        DESTINATION ${CMAKE_INSTALL_FULL_INCLUDEDIR}
)
