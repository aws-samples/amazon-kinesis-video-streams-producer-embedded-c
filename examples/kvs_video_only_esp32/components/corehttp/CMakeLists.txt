cmake_minimum_required(VERSION 3.5)

set(CORE_HTTP_SRC                   "${CMAKE_CURRENT_LIST_DIR}/../../../../libraries/aws/coreHTTP")

set(COMPONENT_SRCS
    ${CORE_HTTP_SRC}/source/core_http_client.c
    ${CORE_HTTP_SRC}/source/dependency/3rdparty/http_parser/http_parser.c
)

set (COMPONENT_ADD_INCLUDEDIRS
    ${CORE_HTTP_SRC}/source/include
    ${CORE_HTTP_SRC}/source/interface
    ${CORE_HTTP_SRC}/source/dependency/3rdparty/http_parser
)

set(COMPONENT_SUBMODULES "corehttp")

register_component()

component_compile_options (
	-Wno-stringop-truncation
	-Os
)