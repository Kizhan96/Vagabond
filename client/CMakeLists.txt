cmake_minimum_required(VERSION 3.14)

project(Vagabond)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

include_directories(src)

set(SOURCES
    src/main.cpp
    src/livekit_window.cpp
    src/livekit_room_widget.cpp
)

set(HEADERS
    src/livekit_window.h
    src/livekit_room_widget.h
)

add_executable(client ${SOURCES} ${HEADERS}
    resources.qrc)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network WebEngineWidgets)

target_link_libraries(client
    Qt6::Core Qt6::Widgets Qt6::Network Qt6::WebEngineWidgets
)

if(WIN32)
    set_target_properties(client PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
