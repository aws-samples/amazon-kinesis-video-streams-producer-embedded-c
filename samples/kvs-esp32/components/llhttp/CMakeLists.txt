cmake_minimum_required(VERSION 3.5)

set(LLHTTP_SRC                   "${CMAKE_CURRENT_LIST_DIR}")

# download release tarball of llhttp if it doesn't exist
if(EXISTS "${LLHTTP_SRC}/src")
else()
    file(MAKE_DIRECTORY "${LLHTTP_SRC}/src")
    file(MAKE_DIRECTORY "${LLHTTP_SRC}/include")
    file(DOWNLOAD
        https://github.com/nodejs/llhttp/archive/release/v4.0.0.tar.gz
        ${CMAKE_CURRENT_BINARY_DIR}/llhttp.tar.gz
    )
    execute_process(COMMAND tar -zxf ${CMAKE_CURRENT_BINARY_DIR}/llhttp.tar.gz -C ${CMAKE_CURRENT_BINARY_DIR})

    set(llhttp_release_folder llhttp-release-v4.0.0)
    file(GLOB llhttp_c_files ${CMAKE_CURRENT_BINARY_DIR}/${llhttp_release_folder}/src/*.c)
    message(STATUS "CMAKE_CURRENT_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}")
    message(STATUS "llhttp c files: ${llhttp_c_files}")
    file(COPY ${llhttp_c_files} DESTINATION ${LLHTTP_SRC}/src)
    file(GLOB llhttp_h_files ${CMAKE_CURRENT_BINARY_DIR}/${llhttp_release_folder}/include/*.h)
    file(COPY ${llhttp_h_files} DESTINATION ${LLHTTP_SRC}/include)
    file(COPY ${CMAKE_CURRENT_BINARY_DIR}/${llhttp_release_folder}/README.md DESTINATION ${LLHTTP_SRC})
    file(COPY ${CMAKE_CURRENT_BINARY_DIR}/${llhttp_release_folder}/LICENSE-MIT DESTINATION ${LLHTTP_SRC})
endif()

set(COMPONENT_SRCS
    ${LLHTTP_SRC}/src/api.c
    ${LLHTTP_SRC}/src/http.c
    ${LLHTTP_SRC}/src/llhttp.c
    ${LLHTTP_SRC}/include/llhttp.h
)

set (COMPONENT_ADD_INCLUDEDIRS
    ${LLHTTP_SRC}/include
)

set(COMPONENT_SUBMODULES "llhttp")

register_component()

component_compile_options (
        -Wno-stringop-truncation
        -Os
)