# Vagabond Server

## Overview

The Vagabond server handles client connections, manages user authentication, facilitates chat and voice communication, and stores message history. It operates as a central hub for multiple clients, ensuring secure and efficient communication.

## Features

- **User Authentication**: Manages user login, registration, and session management.
- **Chat Functionality**: Allows users to send and receive text messages in real-time.
- **Voice Relay**: Forwards voice/audio chunks between clients (no audio capture/output on server).
- **Screen & Video Streams**: Relays screen frames and raw media datagrams with SSRC stamping for smoother client-side playback.
- **Media Chat**: Broadcasts text or media chat messages (caption + base64-encoded payloads) to all connected users and stores concise history entries.
- **Media Presence Signals**: Notifies clients when peers start/stop voice, video, or screen sharing and shares the current active set with newly connected clients.
- **Message History Storage**: Stores and retrieves past messages for users.
- **Defined Message Protocol**: Implements a structured protocol for message encoding and decoding.
- **Browser Viewer Bridge**: Serves MJPEG video plus WAV audio for the active screen sharer at `http://<server>:8080/viewer?user=<username>` so phones can watch without the Qt client. Load the URL with the sharer's username in the query, press **Watch** if it is not auto-started, and the page will subscribe to `/mjpeg/<user>` and `/audio/<user>` streams.
- **Threaded HTTP bridge**: The MJPEG/WAV HTTP helper runs on its own Qt thread so it cannot block TCP or UDP handling while serving large viewers.

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
