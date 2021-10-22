set(WEBRTC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libraries/amazon/amazon-kinesis-video-streams-webrtc-sdk-c)
set(WEBRTC_REPO ${CMAKE_CURRENT_SOURCE_DIR}/libraries/amazon/amazon-kinesis-video-streams-webrtc-sdk-c/webrtc)
set(WEBRTC_PATCH ${CMAKE_CURRENT_SOURCE_DIR}/libraries/amazon/amazon-kinesis-video-streams-webrtc-sdk-c/patch)

#message(STATUS "WEBRTC_REPO                = ${WEBRTC_REPO}")

if (EXISTS "${WEBRTC_REPO}")
else()
    file(MAKE_DIRECTORY "${WEBRTC_REPO}")
    file(DOWNLOAD
        https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c/archive/refs/tags/v1.6.0.tar.gz
        ${CMAKE_CURRENT_BINARY_DIR}/webrtc.tar.gz
    )
    execute_process(COMMAND tar -zxf ${CMAKE_CURRENT_BINARY_DIR}/webrtc.tar.gz -C ${WEBRTC_REPO} --strip-components=1)

    file(
        COPY        ${WEBRTC_PATCH}/CMakeLists.txt
        DESTINATION ${WEBRTC_REPO}
    )

    file(
        COPY        ${WEBRTC_PATCH}/CMake/Dependencies/libwebsockets-old-gcc-fix-cast-cmakelists.patch
        DESTINATION ${WEBRTC_REPO}/CMake/Dependencies
    )

    file(
        COPY        ${WEBRTC_PATCH}/CMake/Utilities.cmake
        DESTINATION ${WEBRTC_REPO}/CMake
    )

    file(
        COPY        ${WEBRTC_PATCH}/src/include/com/amazonaws/kinesis/video/webrtcclient/Include.h
        DESTINATION ${WEBRTC_REPO}/src/include/com/amazonaws/kinesis/video/webrtcclient
    )

    file(
        COPY        ${WEBRTC_PATCH}/src/source/PeerConnection/Rtp.c
        DESTINATION ${WEBRTC_REPO}/src/source/PeerConnection
    )

    file(
        COPY        ${WEBRTC_PATCH}/src/source/Srtp/SrtpSession.c
        DESTINATION ${WEBRTC_REPO}/src/source/Srtp
    )

    file(
        COPY        ${WEBRTC_PATCH}/src/source/Srtp/SrtpSession.h
        DESTINATION ${WEBRTC_REPO}/src/source/Srtp
    )
endif()

SET(USE_OPENSSL OFF CACHE BOOL "USE_OPENSSL OFF")
SET(USE_MBEDTLS ON CACHE BOOL "USE_MBEDTLS ON")
SET(BUILD_SAMPLE OFF CACHE BOOL "BUILD_SAMPLE OFF")

add_subdirectory(${WEBRTC_REPO})