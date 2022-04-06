add_subdirectory(common)

add_subdirectory(kvsapp)

if(${BOARD_INGENIC_T31})
    add_subdirectory(kvsapp-ingenic-t31)
endif()

if(${BUILD_WEBRTC_SAMPLES})
    add_subdirectory(kvs-with-webrtc)
endif()
