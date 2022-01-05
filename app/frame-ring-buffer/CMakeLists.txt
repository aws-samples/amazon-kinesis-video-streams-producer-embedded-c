set(FRAME_RING_BUFFER_SRC
    source/frame_ring_buffer.c
)

set(FRAME_RING_BUFFER_INC
    include
)

# setup static library
add_library(frame-ring-buffer STATIC ${FRAME_RING_BUFFER_SRC})
target_include_directories(frame-ring-buffer PUBLIC ${FRAME_RING_BUFFER_INC})
target_compile_options(frame-ring-buffer PUBLIC --std=c99)
target_link_libraries(frame-ring-buffer PUBLIC
    aziotsharedutil
)
