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

# setup static library
add_library(aziotsharedutil STATIC ${AZURE_C_SHARED_UTILITY_SRC})
target_include_directories(aziotsharedutil PUBLIC ${AZURE_C_SHARED_UTILITY_INC})
if(${USE_WEBRTC_MBEDTLS_LIB})
    target_link_directories(aziotsharedutil PUBLIC ${WEBRTC_LIB_PATH})
    target_include_directories(aziotsharedutil PUBLIC ${WEBRTC_INC_PATH})
endif()
target_link_libraries(aziotsharedutil PUBLIC
    mbedtls mbedcrypto mbedx509
    m
)

include(GNUInstallDirs)

install(TARGETS aziotsharedutil
        LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
)
