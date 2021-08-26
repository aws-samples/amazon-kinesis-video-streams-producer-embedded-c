set(TLSF_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libraries/3rdparty/tlsf)

set(TLSF_SRC
    ${TLSF_DIR}/tlsf.c
    ${TLSF_DIR}/tlsf.h
)

set(TLSF_INC
    ${TLSF_DIR}
)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,--wrap,malloc -Wl,--wrap,free -Wl,--wrap,realloc -Wl,--wrap,calloc")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--wrap,malloc -Wl,--wrap,free -Wl,--wrap,realloc -Wl,--wrap,calloc")
add_definitions(-DKVS_USE_POOL_ALLOCATOR)

# setup shared library
add_library(tlsf-shared SHARED ${TLSF_SRC})
set_target_properties(tlsf-shared PROPERTIES POSITION_INDEPENDENT_CODE 1)
set_target_properties(tlsf-shared PROPERTIES OUTPUT_NAME tlsf)
target_include_directories(tlsf-shared PUBLIC ${TLSF_INC})

# setup static library
add_library(tlsf-static STATIC ${TLSF_SRC})
set_target_properties(tlsf-static PROPERTIES OUTPUT_NAME tlsf)
target_include_directories(tlsf-static PUBLIC ${TLSF_INC})
