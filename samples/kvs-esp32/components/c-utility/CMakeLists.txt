cmake_minimum_required(VERSION 3.5)

set(AZURE_C_SHARED_UTILITY_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../../libraries/3rdparty/c-utility")

set(COMPONENT_SRCS
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

set(COMPONENT_ADD_INCLUDEDIRS
    ${AZURE_C_SHARED_UTILITY_DIR}/inc
    ${AZURE_C_SHARED_UTILITY_DIR}/deps/azure-macro-utils-c/inc
    ${AZURE_C_SHARED_UTILITY_DIR}/deps/umock-c/inc
)

set(COMPONENT_SUBMODULES "c-utility")

register_component()

component_compile_options (
    -Os
)