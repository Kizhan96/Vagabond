cmake_minimum_required(VERSION 3.14)

project(Vagabond)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Include directories
include_directories(src)

# Source files
set(SOURCES
    src/main.cpp
    src/authentication.cpp
    src/chat.cpp
    src/voice.cpp
    src/message_protocol.cpp
    src/history.cpp
    src/window.cpp
    src/screen_share.cpp
    src/video_view.cpp
    src/frame_encode_worker.cpp
    src/h264_encoder.cpp
    src/h264_decoder.cpp
)

set(HEADERS
    src/authentication.h
    src/chat.h
    src/voice.h
    src/message_protocol.h
    src/history.h
    src/window.h
    src/screen_share.h
    src/video_view.h
    src/frame_encode_worker.h
    src/h264_encoder.h
    src/h264_decoder.h
)

# Create the executable
add_executable(client ${SOURCES} ${HEADERS}
    resources.qrc)

# Link against Qt libraries
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Multimedia)
# FFmpeg is located at project root: ../ffmpeg-8.0.1 relative to client/
set(FFMPEG_ROOT "${CMAKE_SOURCE_DIR}/../ffmpeg-8.0.1")
# If that folder doesn't exist (builddir inside client), also try workspace root
if(NOT EXISTS "${FFMPEG_ROOT}")
    set(FFMPEG_ROOT "${CMAKE_SOURCE_DIR}/ffmpeg-8.0.1")
endif()
set(FFMPEG_INCLUDE_DIR "${FFMPEG_ROOT}/include")
set(FFMPEG_LIB_DIR "${FFMPEG_ROOT}/lib")

set(FFMPEG_LIBS
    "${FFMPEG_LIB_DIR}/avdevice.lib"
    "${FFMPEG_LIB_DIR}/avformat.lib"
    "${FFMPEG_LIB_DIR}/avcodec.lib"
    "${FFMPEG_LIB_DIR}/swscale.lib"
    "${FFMPEG_LIB_DIR}/swresample.lib"
    "${FFMPEG_LIB_DIR}/avutil.lib"
)

# Always link packaged FFmpeg .lib files directly (skip DEF generation to avoid missing files).

target_link_libraries(client
    Qt6::Core Qt6::Widgets Qt6::Network Qt6::Multimedia
    ${FFMPEG_LIBS}
)
target_include_directories(client PRIVATE ${FFMPEG_INCLUDE_DIR})
target_link_directories(client PRIVATE ${FFMPEG_LIB_DIR})
message(STATUS "Using FFMPEG_ROOT=${FFMPEG_ROOT}")
message(STATUS "FFmpeg include dir: ${FFMPEG_INCLUDE_DIR}")
message(STATUS "FFmpeg lib dir: ${FFMPEG_LIB_DIR}")
message(STATUS "FFmpeg libs: ${FFMPEG_LIBS}")

if(WIN32)
    set_target_properties(client PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
