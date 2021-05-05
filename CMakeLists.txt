cmake_minimum_required( VERSION 3.2.0 )

project( kvs-producer-rtos
	 VERSION 202012.00
	 LANGUAGES C )

# Do not allow in-source build.
if( ${PROJECT_SOURCE_DIR} STREQUAL ${PROJECT_BINARY_DIR} )
    message( FATAL_ERROR "In-source build is not allowed. Please build in a separate directory, such as ${PROJECT_SOURCE_DIR}/build." )
endif()

# Configurations for mbedtls
set(ENABLE_PROGRAMS Off)
set(ENABLE_TESTING Off)
set(USE_SHARED_MBEDTLS_LIBRARY Off)
set(USE_STATIC_MBEDTLS_LIBRARY On)

# Add AWS libraries
add_subdirectory( libraries/aws/coreHTTP )

# Add 3rd party libraries
add_subdirectory( libraries/3rdparty/mbedtls )
add_subdirectory( libraries/3rdparty/parson )

option(ENABLE_DEBUG_LOG "enable debug log" On)

# Add sources
add_subdirectory( src )

# Add examples
add_subdirectory( examples/kvs_video_only_linux )
add_subdirectory( examples/kvs_audio_video_linux )