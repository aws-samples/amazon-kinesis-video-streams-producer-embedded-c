set(PARSON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libraries/3rdparty/parson)

set(PARSON_SRC
    ${PARSON_DIR}/parson.c
    ${PARSON_DIR}/parson.h
)

set(PARSON_INC
    ${PARSON_DIR}
)

# setup static library
add_library(parson STATIC ${PARSON_SRC})
target_include_directories(parson PUBLIC ${PARSON_INC})

include(GNUInstallDirs)

install(TARGETS parson
        LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_FULL_INCLUDEDIR}
)
