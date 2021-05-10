cmake_minimum_required(VERSION 3.5)

set(PARSON_SRC "${CMAKE_CURRENT_LIST_DIR}/../../../../libraries/3rdparty/parson")

set(COMPONENT_SRCS
    ${PARSON_SRC}/parson.c
)

set(COMPONENT_ADD_INCLUDEDIRS
    ${PARSON_SRC}
)

set(COMPONENT_SUBMODULES "parson")

register_component()

component_compile_options (
    -Os
)
