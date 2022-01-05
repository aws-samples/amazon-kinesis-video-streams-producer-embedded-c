set(TLSF_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libraries/3rdparty/tlsf)

set(TLSF_SRC
    ${TLSF_DIR}/tlsf.c
    ${TLSF_DIR}/tlsf.h
)

set(TLSF_INC
    ${TLSF_DIR}
)

# setup static library
add_library(tlsf STATIC ${TLSF_SRC})
target_include_directories(tlsf PUBLIC ${TLSF_INC})

include(GNUInstallDirs)

install(TARGETS tlsf
        LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
)
