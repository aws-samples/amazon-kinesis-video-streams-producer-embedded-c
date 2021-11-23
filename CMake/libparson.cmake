set(PARSON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libraries/3rdparty/parson)

set(PARSON_SRC
    ${PARSON_DIR}/parson.c
    ${PARSON_DIR}/parson.h
)

set(PARSON_INC
    ${PARSON_DIR}
)

# setup library interface
add_library(parson INTERFACE)
set_target_properties(parson PROPERTIES PUBLIC_HEADER ${PARSON_INC}/parson.h)
target_include_directories(parson INTERFACE ${PARSON_INC})

# setup shared library
add_library(parson-shared SHARED ${PARSON_SRC})
set_target_properties(parson-shared PROPERTIES POSITION_INDEPENDENT_CODE 1)
set_target_properties(parson-shared PROPERTIES OUTPUT_NAME parson)
target_include_directories(parson-shared PUBLIC ${PARSON_INC})

# setup static library
add_library(parson-static STATIC ${PARSON_SRC})
set_target_properties(parson-static PROPERTIES OUTPUT_NAME parson)
target_include_directories(parson-static PUBLIC ${PARSON_INC})

include(GNUInstallDirs)

install(TARGETS parson
        LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_FULL_INCLUDEDIR}
)

install(TARGETS parson-shared
        LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
)

install(TARGETS parson-static
        LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
)
