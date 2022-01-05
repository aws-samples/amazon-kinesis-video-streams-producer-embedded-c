# build azure c shared utility
set(SAMPLES_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(SAMPLES_COMMON_SRC
    ${SAMPLES_COMMON_DIR}/aac_file_loader.c
    ${SAMPLES_COMMON_DIR}/aac_file_loader.h
    ${SAMPLES_COMMON_DIR}/file_input_stream.c
    ${SAMPLES_COMMON_DIR}/file_input_stream.h
    ${SAMPLES_COMMON_DIR}/file_io.c
    ${SAMPLES_COMMON_DIR}/file_io.h
    ${SAMPLES_COMMON_DIR}/file_loader.h
    ${SAMPLES_COMMON_DIR}/g711_file_loader.c
    ${SAMPLES_COMMON_DIR}/g711_file_loader.h
    ${SAMPLES_COMMON_DIR}/h264_file_loader.c
    ${SAMPLES_COMMON_DIR}/h264_file_loader.h
)

set(SAMPLES_COMMON_INC
    ${SAMPLES_COMMON_DIR}
)

# setup static library
add_library(samplescommon STATIC ${SAMPLES_COMMON_SRC})
target_include_directories(samplescommon PUBLIC ${SAMPLES_COMMON_INC})
target_link_libraries(samplescommon PUBLIC
    kvs-embedded-c
    aziotsharedutil
)