# build azure c shared utility
set(SAMPLES_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(SAMPLES_COMMON_SRC
    ${SAMPLES_COMMON_DIR}/aac_file_loader.c
    ${SAMPLES_COMMON_DIR}/file_io.c
    ${SAMPLES_COMMON_DIR}/file_input_stream.c
    ${SAMPLES_COMMON_DIR}/h264_file_loader.c
)

set(SAMPLES_COMMON_INC
    ${SAMPLES_COMMON_DIR}
)

# setup shared library
add_library(samplescommon-shared SHARED ${SAMPLES_COMMON_SRC})
set_target_properties(samplescommon-shared PROPERTIES POSITION_INDEPENDENT_CODE 1)
set_target_properties(samplescommon-shared PROPERTIES OUTPUT_NAME samplescommon)
target_include_directories(samplescommon-shared PUBLIC ${SAMPLES_COMMON_INC})
target_link_libraries(samplescommon-shared PUBLIC
    kvs-embedded-c-shared
    aziotsharedutil-shared
)

# setup static library
add_library(samplescommon-static STATIC ${SAMPLES_COMMON_SRC})
set_target_properties(samplescommon-static PROPERTIES OUTPUT_NAME samplescommon)
target_include_directories(samplescommon-static PUBLIC ${SAMPLES_COMMON_INC})
target_link_libraries(samplescommon-static PUBLIC
    kvs-embedded-c-static
    aziotsharedutil-static
)