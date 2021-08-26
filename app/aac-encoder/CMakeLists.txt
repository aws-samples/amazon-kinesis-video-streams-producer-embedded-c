# compiled as C99
set(CMAKE_C_FLAGS "--std=c99 ${CMAKE_C_FLAGS}")

# support usleep
set(CMAKE_C_FLAGS "-D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=200112L ${CMAKE_C_FLAGS}")

set(AAC_ENCODER_SRC
    source/aac_encoder.c
)

set(AAC_ENCODER_INC
    include
)

# setup shared library
add_library(aac-encoder-shared SHARED ${AAC_ENCODER_SRC})
set_target_properties(aac-encoder-shared PROPERTIES OUTPUT_NAME aac-encoder)
set_target_properties(aac-encoder-shared PROPERTIES POSITION_INDEPENDENT_CODE 1)
target_include_directories(aac-encoder-shared PUBLIC ${AAC_ENCODER_INC})
target_link_libraries(aac-encoder-shared PUBLIC
    fdk-aac-shared kvs-embedded-c-shared aziotsharedutil-shared
)

# setup static library
add_library(aac-encoder-static STATIC ${AAC_ENCODER_SRC})
set_target_properties(aac-encoder-static PROPERTIES OUTPUT_NAME aac-encoder)
target_include_directories(aac-encoder-static PUBLIC ${AAC_ENCODER_INC})
target_link_libraries(aac-encoder-static PUBLIC
    fdk-aac-static kvs-embedded-c-static aziotsharedutil-static
)
