cmake_minimum_required(VERSION 3.5)

set(SAMPLES_COMMON_SRC "${CMAKE_CURRENT_LIST_DIR}/../../../common")

set(COMPONENT_SRCS
    ${SAMPLES_COMMON_SRC}/aac_file_loader.c
    ${SAMPLES_COMMON_SRC}/file_io.c
    ${SAMPLES_COMMON_SRC}/h264_file_loader.c
)

set(COMPONENT_ADD_INCLUDEDIRS
    ${SAMPLES_COMMON_SRC}
)

set(COMPONENT_SUBMODULES "samples-common")

set(COMPONENT_PRIV_REQUIRES "kvs-embedded-c")

register_component()

component_compile_options (
    -Os
)

