message("BOARD=${BOARD}")

if("${BOARD}" STREQUAL "OFF")
    add_subdirectory(common)
endif()

if("${BOARD}" STREQUAL "OFF")
    add_subdirectory(kvs-linux)
endif()

if("${BOARD}" STREQUAL "OFF")
    add_subdirectory(mkv_uploader)
endif()

if("${BOARD}" STREQUAL "OFF")
    add_subdirectory(kvsapp)
endif()

if("${BOARD}" STREQUAL "INGENTIC_T31")
    add_subdirectory(kvsapp-ingenic-t31)
endif()
