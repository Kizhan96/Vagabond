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
4. **VoiceChunk (4)**: (TCP fallback, deprecated) Audio chunks now ride over UDP media datagrams with `mediaType=0`.
5. **LogoutRequest (5)**: Client → server, asks to end the session.
6. **HistoryRequest (6)**: Client → server, may include channel/user scope in payload.
7. **HistoryResponse (7)**: Server → client, payload holds serialized history page.
8. **UsersListRequest (8)**: Client → server, request list of connected users.
9. **UsersListResponse (9)**: Server → client, payload is newline-separated list of users.
10. **ScreenFrame (10)**: Stream metadata/beacons on TCP; actual frames travel via UDP media datagrams with `mediaType=1` and a 4-byte frame id prefix inside the payload. Special ids when sent over TCP for coordination:
    - `0xFFFFFFFF` – presence/beacon ("I'm streaming")
    - `0xFFFFFFFE` – explicit stop marker
11. **StreamAudio (11)**: (TCP fallback, deprecated) Screen-share audio now rides over UDP media datagrams with `mediaType=2` and a mini header `seq (u32) + ts (qint64)` preceding PCM data.
12. **UdpPortsAnnouncement (12)**: Client → server, announces local UDP ports for voice/video relay.
13. **ChatMedia (13)**: Bidirectional media message; payload JSON `{ "mime": "image/png", "text": "caption", "dataBase64": "..." }`.
14. **MediaControl (14)**: Start/stop presence updates for streams (`{"kind":"screen|video|voice","state":"start|stop","from":"user"}`) and snapshots (`{"snapshot":true,"active":[...]}`).
15. **Ping (15)** / **Pong (16)**: Health check round trips; payload is opaque.
17. **WebFrame (17)**: JPEG frame for the HTTP bridge so browser viewers can see the shared screen without the native client.
255. **Error (255)**: Error description in payload.

## UDP media datagrams

Voice, screen video, and stream-audio now travel exclusively over UDP. Each datagram begins with a `MediaHeader` encoded in
big-endian order:

```
| version (u8) | mediaType (u8) | codec (u8) | flags (u8) | ssrc (u32) | timestampMs (u32) | seq (u16) | payloadLen (u16) | payload |
```

- `mediaType`: `0` voice, `1` video/screen frames, `2` stream audio accompanying a share.
- `codec`: `0` raw/Opus audio, `1` H.264 video.
- Payloads for video start with a 4-byte frame id followed by H.264 bytes. Stream audio payloads begin with `seq (u32) + ts (qint64)`
  before PCM data to aid de-jittering.

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
