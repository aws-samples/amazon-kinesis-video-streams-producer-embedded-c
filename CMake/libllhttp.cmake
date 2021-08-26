set(LLHTTP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libraries/3rdparty/llhttp)

set(LLHTTP_SRC
    ${LLHTTP_DIR}/src/api.c
    ${LLHTTP_DIR}/src/http.c
    ${LLHTTP_DIR}/src/llhttp.c
    ${LLHTTP_DIR}/include/llhttp.h
)

set(LLHTTP_INC
    ${LLHTTP_DIR}/include
)

# setup shared library
add_library(llhttp-shared SHARED ${LLHTTP_SRC})
set_target_properties(llhttp-shared PROPERTIES POSITION_INDEPENDENT_CODE 1)
set_target_properties(llhttp-shared PROPERTIES OUTPUT_NAME llhttp)
target_include_directories(llhttp-shared PUBLIC ${LLHTTP_INC})

# setup static library
add_library(llhttp-static STATIC ${LLHTTP_SRC})
set_target_properties(llhttp-static PROPERTIES OUTPUT_NAME llhttp)
target_include_directories(llhttp-static PUBLIC ${LLHTTP_INC})
