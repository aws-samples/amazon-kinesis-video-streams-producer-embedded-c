message("BOARD=${BOARD}")

if(NOT "${BOARD}")
    add_subdirectory(common)
endif()

if(NOT "${BOARD}")
    add_subdirectory(kvs-linux)
endif()

if(NOT "${BOARD}")
    add_subdirectory(mkv_uploader)
endif()

if(NOT "${BOARD}")
    add_subdirectory(kvsapp)
endif()

if("${BOARD}" STREQUAL "INGENTIC_T31")
    add_subdirectory(kvsapp-ingenic-t31)
endif()
