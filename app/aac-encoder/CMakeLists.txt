set(AAC_ENCODER_SRC
    source/aac_encoder.c
)

set(AAC_ENCODER_INC
    include
)

# setup static library
add_library(aac-encoder STATIC ${AAC_ENCODER_SRC})
target_include_directories(aac-encoder PUBLIC ${AAC_ENCODER_INC})
target_compile_options(aac-encoder PUBLIC --std=c99)
target_link_libraries(aac-encoder PUBLIC
    fdk-aac
)
