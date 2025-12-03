cmake_minimum_required(VERSION 3.14)

project(QtClientServerApp)

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
    src/dxgi_capture.cpp
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
    src/dxgi_capture.h
    src/frame_encode_worker.h
    src/h264_encoder.h
    src/h264_decoder.h
)

# Create the executable
add_executable(client ${SOURCES} ${HEADERS}
    resources.qrc)

# Link against Qt libraries
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Multimedia)
set(FFMPEG_ROOT "C:/Users/zabiv/AppData/Local/Microsoft/WinGet/Packages/Gyan.FFmpeg.Shared_Microsoft.Winget.Source_8wekyb3d8bbwe/ffmpeg-8.0.1-full_build-shared")
set(FFMPEG_INCLUDE_DIR "${FFMPEG_ROOT}/include")
set(AVCODEC_LIBRARY "${FFMPEG_ROOT}/lib/avcodec.lib")
set(AVUTIL_LIBRARY "${FFMPEG_ROOT}/lib/avutil.lib")
set(SWSCALE_LIBRARY "${FFMPEG_ROOT}/lib/swscale.lib")

target_link_libraries(client
    Qt6::Core Qt6::Widgets Qt6::Network Qt6::Multimedia
    d3d11 dxgi
    ${AVCODEC_LIBRARY} ${AVUTIL_LIBRARY} ${SWSCALE_LIBRARY}
)
target_include_directories(client PRIVATE ${FFMPEG_INCLUDE_DIR})

if(WIN32)
    set_target_properties(client PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
