# Message Protocol Documentation

## Overview
This document outlines the message protocol used in the client-server application, detailing the structure of messages exchanged between clients and the server, as well as the defined message types.

## Message Structure
Each message sent between the client and server is length-prefixed to avoid TCP packet sticking and to validate input:

```
| FrameLength (uint32 LE) | MessageType (uint8) | Sender (QString) | Recipient (QString) | Payload (QByteArray) | TimestampMs (qint64) |
```

- **FrameLength**: Number of bytes that follow (everything after this field).
- **MessageType**: Enumeration of message kinds.
- **Sender / Recipient**: UTF-16 strings encoded by `QDataStream`.
- **Payload**: For text, UTF-8 bytes; for voice, raw or encoded audio chunk.
- **TimestampMs**: Unix epoch milliseconds set by the sender.

## Message Types
The following message types are defined in the `MessageType` enum:

1. **LoginRequest (1)**: Client → server, includes credentials in payload.
   - Payload: JSON `{ "username": "...", "password": "...", "register": true|false }`
2. **LoginResponse (2)**: Server → client, payload contains `"ok"` or error text; may carry a token.
3. **ChatMessage (3)**: Bidirectional text message; payload is UTF-8 text.
4. **VoiceChunk (4)**: Bidirectional audio chunk (e.g., PCM/Opus frame).
5. **LogoutRequest (5)**: Client → server, asks to end the session.
6. **HistoryRequest (6)**: Client → server, may include channel/user scope in payload.
7. **HistoryResponse (7)**: Server → client, payload holds serialized history page.
8. **UsersListRequest (8)**: Client → server, request list of connected users.
9. **UsersListResponse (9)**: Server → client, payload is newline-separated list of users.
10. **ScreenFrame (10)**: Bidirectional screen frame; payload is compressed image bytes (e.g., JPEG).
255. **Error (255)**: Error description in payload.

## Example Message
### Sending a Chat Message
When a client sends a chat message, the message structure would look like this:

```
| FrameLength | MessageType | Sender | Recipient | Payload | TimestampMs |
|     47      |   3         | "alice"| "room1"   | "Hello, World!" | 1724261100123 |
```

### Receiving a Login Response
When the server responds to a login request, the message structure might be:

```
| FrameLength | MessageType | Sender | Recipient | Payload | TimestampMs |
|     38      |   2         | "server" | "" | "Success" | 1724261100456 |
```

## Conclusion
This protocol ensures that messages are structured consistently, allowing for reliable communication between clients and the server. Each message type is designed to facilitate specific functionalities within the application, such as authentication, chat, and voice communication.
