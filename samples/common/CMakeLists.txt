# build azure c shared utility
set(SAMPLES_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_library(samplescommon
    ${SAMPLES_COMMON_DIR}/aac_file_loader.c
    ${SAMPLES_COMMON_DIR}/file_io.c
    ${SAMPLES_COMMON_DIR}/file_input_stream.c
    ${SAMPLES_COMMON_DIR}/h264_file_loader.c
)

target_include_directories(samplescommon PUBLIC
    ${SAMPLES_COMMON_DIR}
)

target_link_libraries(samplescommon
    kvs-embedded-c
)