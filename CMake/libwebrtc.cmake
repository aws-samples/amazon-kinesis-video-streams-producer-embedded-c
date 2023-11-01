set(WEBRTC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libraries/amazon/amazon-kinesis-video-streams-webrtc-sdk-c)
set(WEBRTC_REPO ${CMAKE_CURRENT_SOURCE_DIR}/libraries/amazon/amazon-kinesis-video-streams-webrtc-sdk-c/webrtc)
set(WEBRTC_PATCH ${CMAKE_CURRENT_SOURCE_DIR}/libraries/amazon/amazon-kinesis-video-streams-webrtc-sdk-c/patch)

#message(STATUS "WEBRTC_REPO                = ${WEBRTC_REPO}")

if (EXISTS "${WEBRTC_REPO}")
else()
    file(MAKE_DIRECTORY "${WEBRTC_REPO}")
    file(DOWNLOAD
        https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c/archive/refs/tags/v1.8.1.tar.gz
        ${CMAKE_CURRENT_BINARY_DIR}/webrtc.tar.gz
    )
    execute_process(COMMAND tar -zxf ${CMAKE_CURRENT_BINARY_DIR}/webrtc.tar.gz -C ${WEBRTC_REPO} --strip-components=1)

    file(
            COPY        ${WEBRTC_PATCH}/CMakeLists.txt
            DESTINATION ${WEBRTC_REPO}
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
            COPY        ${WEBRTC_PATCH}/src/source/PeerConnection/Retransmitter.c
            DESTINATION ${WEBRTC_REPO}/src/source/PeerConnection
    )

    file(
            COPY        ${WEBRTC_PATCH}/src/source/PeerConnection/Rtp.c
            DESTINATION ${WEBRTC_REPO}/src/source/PeerConnection
    )

    file(
            COPY        ${WEBRTC_PATCH}/src/source/Rtcp/RtpRollingBuffer.c
            DESTINATION ${WEBRTC_REPO}/src/source/Rtcp
    )

    file(
            COPY        ${WEBRTC_PATCH}/src/source/Rtcp/RtpRollingBuffer.h
            DESTINATION ${WEBRTC_REPO}/src/source/Rtcp
    )

    file(
            COPY        ${WEBRTC_PATCH}/src/source/Rtp/RtpPacket.h
            DESTINATION ${WEBRTC_REPO}/src/source/Rtp
    )

    file(
            COPY        ${WEBRTC_PATCH}/src/source/Rtp/Codecs/RtpG711Payloader.c
            DESTINATION ${WEBRTC_REPO}/src/source/Rtp/Codecs
    )

    file(
            COPY        ${WEBRTC_PATCH}/src/source/Rtp/Codecs/RtpH264Payloader.c
            DESTINATION ${WEBRTC_REPO}/src/source/Rtp/Codecs
    )

    file(
            COPY        ${WEBRTC_PATCH}/src/source/Rtp/Codecs/RtpOpusPayloader.c
            DESTINATION ${WEBRTC_REPO}/src/source/Rtp/Codecs
    )

    file(
            COPY        ${WEBRTC_PATCH}/src/source/Rtp/Codecs/RtpVP8Payloader.c
            DESTINATION ${WEBRTC_REPO}/src/source/Rtp/Codecs
    )

    file(
            COPY        ${WEBRTC_PATCH}/src/source/Srtp/SrtpSession.c
            DESTINATION ${WEBRTC_REPO}/src/source/Srtp
    )
endif()

add_subdirectory(${WEBRTC_REPO})
