if(${BUILD_SHARED_SAMPLES} OR ${BUILD_STATIC_SAMPLES})
    add_subdirectory(common)
endif()

if(${BUILD_SHARED_SAMPLES} OR ${BUILD_STATIC_SAMPLES})
    add_subdirectory(kvs-linux)
endif()

if(${BUILD_SHARED_SAMPLES} OR ${BUILD_STATIC_SAMPLES})
    add_subdirectory(mkv_uploader)
endif()

if(${BUILD_SHARED_SAMPLES} OR ${BUILD_STATIC_SAMPLES})
    add_subdirectory(kvsapp)
endif()

if(${BOARD_INGENIC_T31})
    add_subdirectory(kvsapp-ingenic-t31)
endif()
