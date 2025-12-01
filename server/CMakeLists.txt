project(qt-client-server-app)

cmake_minimum_required(VERSION 3.14)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)

include_directories(src)

set(SERVER_SOURCES
    src/main.cpp
    src/authentication.cpp
    src/chat.cpp
    src/message_protocol.cpp
    src/history.cpp
    src/telegrambot.cpp
    src/telegramlinks.cpp
    src/telegrambot.h
    src/telegramlinks.h
)

add_executable(server ${SERVER_SOURCES})

find_package(Qt6 REQUIRED COMPONENTS Core Network)

target_link_libraries(server Qt6::Core Qt6::Network)
