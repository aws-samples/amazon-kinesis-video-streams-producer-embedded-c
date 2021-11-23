set(AZURE_C_SHARED_UTILITY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libraries/3rdparty/c-utility)

set(AZURE_C_SHARED_UTILITY_SRC
    ${AZURE_C_SHARED_UTILITY_DIR}/adapters/lock_pthreads.c
    ${AZURE_C_SHARED_UTILITY_DIR}/src/buffer.c
    ${AZURE_C_SHARED_UTILITY_DIR}/src/consolelogger.c
    ${AZURE_C_SHARED_UTILITY_DIR}/src/crt_abstractions.c
    ${AZURE_C_SHARED_UTILITY_DIR}/src/doublylinkedlist.c
    ${AZURE_C_SHARED_UTILITY_DIR}/src/httpheaders.c
    ${AZURE_C_SHARED_UTILITY_DIR}/src/map.c
    ${AZURE_C_SHARED_UTILITY_DIR}/src/strings.c
    ${AZURE_C_SHARED_UTILITY_DIR}/src/xlogging.c
)

set(AZURE_C_SHARED_UTILITY_INC
    ${AZURE_C_SHARED_UTILITY_DIR}/inc
    ${AZURE_C_SHARED_UTILITY_DIR}/deps/azure-macro-utils-c/inc
    ${AZURE_C_SHARED_UTILITY_DIR}/deps/umock-c/inc
)

# setup library interface
add_library(aziotsharedutil INTERFACE)
target_include_directories(aziotsharedutil INTERFACE ${AZURE_C_SHARED_UTILITY_INC})

# setup shared library
add_library(aziotsharedutil-shared SHARED ${AZURE_C_SHARED_UTILITY_SRC})
set_target_properties(aziotsharedutil-shared PROPERTIES POSITION_INDEPENDENT_CODE 1)
set_target_properties(aziotsharedutil-shared PROPERTIES OUTPUT_NAME aziotsharedutil)
target_include_directories(aziotsharedutil-shared PUBLIC ${AZURE_C_SHARED_UTILITY_INC})
target_link_libraries(aziotsharedutil-shared PUBLIC
    mbedtls mbedcrypto mbedx509
    m
)

# setup static library
add_library(aziotsharedutil-static STATIC ${AZURE_C_SHARED_UTILITY_SRC})
set_target_properties(aziotsharedutil-static PROPERTIES OUTPUT_NAME aziotsharedutil)
target_include_directories(aziotsharedutil-static PUBLIC ${AZURE_C_SHARED_UTILITY_INC})
target_link_libraries(aziotsharedutil-static PUBLIC
    mbedtls mbedcrypto mbedx509
    m
)

include(GNUInstallDirs)

install(TARGETS aziotsharedutil
        LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
)

install(TARGETS aziotsharedutil-shared
        LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
)

install(TARGETS aziotsharedutil-static
        LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
)
