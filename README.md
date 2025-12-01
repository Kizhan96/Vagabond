# Vagabond/Vagabond/README.md

# Qt Client-Server Application

This project is a client-server application built using Qt 6.9.1 with C++. It includes user authentication, voice communication, chat functionality, message history storage, and a defined message protocol.

## Features

- **User Authentication**: Secure login and registration for users.
- **Voice Communication**: Real-time voice transmission between clients.
- **Chat Functionality**: Send and receive text messages in real-time.
- **Message History Storage**: Access to past messages for both chat and voice.
- **Defined Message Protocol**: A structured protocol for message encoding and decoding.

## Project Structure

```
Vagabond
├── client
│   ├── src
│   ├── CMakeLists.txt
│   └── README.md
├── server
│   ├── src
│   ├── CMakeLists.txt
│   └── README.md
├── protocol
│   ├── protocol.md
│   └── README.md
└── README.md
```

## Setup Instructions

1. **Clone the repository**:
   ```
   git clone <repository-url>
   cd Vagabond
   ```

2. **Build the Client**:
   - Navigate to the `client` directory.
   - Run CMake to configure the project:
     ```
     mkdir build
     cd build
     cmake ..
     ```
   - Build the client application:
     ```
     cmake --build .
     ```

3. **Build the Server**:
   - Navigate to the `server` directory.
   - Repeat the CMake configuration and build steps as done for the client.

4. **Run the Server**:
   - Execute the server application from the build directory. It listens on TCP `0.0.0.0:12345` and stores users in `users.json`, history in `history.log`.

5. **Run the Client**:
   - Execute the client application from its build directory. GUI позволяет ввести хост/порт/логин/пароль, отправлять сообщения и загружать историю.
   - Можно предварительно заполнить поля через переменные окружения:
     - `APP_HOST` (default `127.0.0.1`)
     - `APP_PORT` (default `12345`)
     - `APP_USER` (default `demo`)
     - `APP_PASS` (default `demo`)
     - `APP_REGISTER` (`1` to register then login, `0` to only login)
   - Пример:
     ```
     set APP_USER=alice
     set APP_PASS=secret
     set APP_REGISTER=1
     ./client.exe
     ```

## Usage

- After starting the server, launch the client application.
- Users can register and log in to access chat and voice features.
- Messages and voice data are transmitted in real-time, and users can view their message history.

## Documentation

For detailed documentation on the client and server implementations, refer to the respective `README.md` files in the `client` and `server` directories. The `protocol.md` file in the `protocol` directory provides an in-depth explanation of the message protocol used in this application.
