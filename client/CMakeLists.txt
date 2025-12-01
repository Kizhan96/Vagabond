cmake_minimum_required(VERSION 3.14)

project(QtClientServerApp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)

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
)

set(HEADERS
    src/authentication.h
    src/chat.h
    src/voice.h
    src/message_protocol.h
    src/history.h
    src/window.h
    src/screen_share.h
)

# Create the executable
add_executable(client ${SOURCES} ${HEADERS})

# Link against Qt libraries
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Multimedia)

target_link_libraries(client Qt6::Core Qt6::Widgets Qt6::Network Qt6::Multimedia)

if(WIN32)
    set_target_properties(client PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
