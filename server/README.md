# Vagabond Server

## Overview

**Deprecated**: The project has moved to LiveKit for signaling and media. The Qt client no longer depends on this custom server. Keep this code only if you need the legacy TCP/UDP implementation for reference.

## Features

- **User Authentication**: Manages user login, registration, and session management.
- **Chat Functionality**: Allows users to send and receive text messages in real-time.
- **Voice Relay**: Forwards voice/audio chunks between clients (no audio capture/output on server).
- **Message History Storage**: Stores and retrieves past messages for users.
- **Defined Message Protocol**: Implements a structured protocol for message encoding and decoding.

## Setup Instructions

1. **Dependencies**: Ensure that you have Qt 6.9.1 (Core + Network) installed on your system.
2. **Build the Vagabond server**:
   - Navigate to the `server` directory.
   - Run the following commands:
     ```
     mkdir build
     cd build
     cmake ..
     make
     ```
3. **Run the Server**: After building, execute the server binary to start listening for client connections. By default it binds to `0.0.0.0:12345`, stores users in `users.json` and chat history in `history.log` in the working directory.

## Usage

- Start the server application before launching any client applications.
- Clients will connect to the server using the specified IP address and port.
- Follow the client documentation for instructions on connecting and using the chat and voice features.

## Message Protocol

Refer to the `protocol/protocol.md` file for detailed information on the message structure and types used in communication between the server and clients. 

## Contributing

For contributions, please follow the project's coding standards and guidelines. Ensure to test your changes thoroughly before submitting a pull request.

## License

This project is licensed under the MIT License. See the LICENSE file for more details.
