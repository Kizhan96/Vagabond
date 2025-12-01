# Client Application README

# Qt Client-Server Application

This is the client component of the Qt Client-Server Application, which provides user authentication, voice communication, chat functionality, and message history storage.

## Features

- **User Authentication**: Users can register and log in to the application.
- **Voice Communication**: Real-time voice communication between users.
- **Chat Functionality**: Send and receive text messages.
- **Message History Storage**: Access past messages for reference.
- **Defined Message Protocol**: Structured communication between client and server.

## Setup Instructions

1. **Clone the Repository**:
   ```
   git clone <repository-url>
   cd qt-client-server-app/client
   ```

2. **Build the Application**:
   Ensure you have CMake and Qt installed. Run the following commands:
   ```
   mkdir build
   cd build
   cmake ..
   make
   ```

3. **Run the Application**:
   After building, you can run the client application. Configure connection via environment variables:
   - `APP_HOST` (default `127.0.0.1`)
   - `APP_PORT` (default `12345`)
   - `APP_USER` (default `demo`)
   - `APP_PASS` (default `demo`)
   - `APP_REGISTER` (`1` to register then login, `0` to only login`)

   Example (Windows PowerShell):
   ```
   $env:APP_USER="alice"
   $env:APP_PASS="secret"
   $env:APP_REGISTER="1"
   ./qt-client
   ```

## Usage

- **Login**: Enter your credentials to log in.
- **Chat**: Use the chat interface to send and receive messages.
- **Voice**: Start voice communication by selecting the appropriate option in the interface.

## Contributing

Feel free to submit issues or pull requests for improvements and bug fixes.

## License

This project is licensed under the MIT License. See the LICENSE file for details.
