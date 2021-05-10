cmake_minimum_required(VERSION 2.6)

project(llhttp)

# download release tarball of llhttp if it doesn't exist
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src")
else()
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include")
    file(DOWNLOAD
        https://github.com/nodejs/llhttp/archive/release/v4.0.0.tar.gz
        ${CMAKE_CURRENT_BINARY_DIR}/llhttp.tar.gz
    )
    execute_process(COMMAND tar -zxf ${CMAKE_CURRENT_BINARY_DIR}/llhttp.tar.gz -C ${CMAKE_CURRENT_BINARY_DIR})

    set(llhttp_release_folder llhttp-release-v4.0.0)
    file(GLOB llhttp_c_files ${CMAKE_CURRENT_BINARY_DIR}/${llhttp_release_folder}/src/*.c)
    message(STATUS "CMAKE_CURRENT_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}")
    message(STATUS "llhttp c files: ${llhttp_c_files}")
    file(COPY ${llhttp_c_files} DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/src)
    file(GLOB llhttp_h_files ${CMAKE_CURRENT_BINARY_DIR}/${llhttp_release_folder}/include/*.h)
    file(COPY ${llhttp_h_files} DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/include)
    file(COPY ${CMAKE_CURRENT_BINARY_DIR}/${llhttp_release_folder}/README.md DESTINATION ${CMAKE_CURRENT_SOURCE_DIR})
    file(COPY ${CMAKE_CURRENT_BINARY_DIR}/${llhttp_release_folder}/LICENSE-MIT DESTINATION ${CMAKE_CURRENT_SOURCE_DIR})
endif()

add_library(llhttp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/api.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/http.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/llhttp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/include/llhttp.h
)

target_include_directories(llhttp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)