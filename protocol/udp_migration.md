# UDP media migration plan

This document sketches how to re-introduce UDP for real-time media while keeping the existing TCP channel for signaling and text chat. The goal is to move voice and screen-sharing video to low-latency datagram transports similar to Jitsi/Discord.

## Transport layout
- **TCP (existing):** authentication, presence, chat messages, history sync, user list, and control signaling (SDP-like negotiation and candidate exchange for media sockets).
- **UDP/Voice:** dedicated port for Opus-encoded audio frames. Server relays to subscribed peers (fan-out) or optionally mixes. Each datagram carries a small media header with sequence number, timestamp (ms), codec ID, and payload length.
- **UDP/Video:** dedicated port for screen-share/other video frames. Clients send only the latest encoded frame (dropping older ones). Header mirrors the voice header with stream ID to allow multiple publishers.
- **Per-client sockets:** clients open two `QUdpSocket` instances (voice/video) bound to ephemeral ports and announce them over the TCP signaling channel. The server records the remote address/port per user and relays datagrams accordingly.

## Message framing (draft)
```
struct MediaHeader {
    quint8  version = 1;
    quint8  mediaType;    // 0 = voice, 1 = video
    quint8  codec;        // 0 = Opus, 1 = H.264
    quint8  flags;        // bit0: keyframe (video), bit1: marker (end of frame)
    quint32 ssrc;         // sender ID
    quint32 timestampMs;  // capture time since epoch ms
    quint16 seq;          // increments per packet
    quint16 payloadLen;   // length of payload following header
};
```
- Entire packet: `MediaHeader` (packed, network byte order) + raw encoded payload.
- UDP datagrams are limited to ~1200 bytes for safer traversal; video/voice encoders should packetize accordingly.
- If a datagram arrives out-of-order or is missing, receivers use seq/timestamp to drop or conceal; no TCP-style buffering.

## Server responsibilities
- Maintain maps of user -> {voiceEndpoint, videoEndpoint} announced over TCP.
- For each incoming datagram on the UDP sockets, look up subscribed peers and forward the payload with unchanged headers (acting as an SFU). No re-encoding is required initially.
- Apply rate limiting per sender to mitigate abuse and prevent buffer bloat.

## Client responsibilities
- On login, send TCP control messages that advertise the local UDP ports for voice and video.
- Send encoded audio via the voice socket; if `bytesToWrite()` on TCP grows, still continue streaming over UDP (no head-of-line blocking).
- For video, always send the most recent encoded frame. If the encoder produces a new frame while the previous one is still in flight, drop the older frame to keep latency low.
- On receive, maintain short jitter buffers (e.g., 60–120 ms for audio, 1–3 frames for video) keyed by `seq`/`timestampMs` before handing to the decoder.

## Incremental adoption steps
1. Add minimal `QUdpSocket` plumbing to server (bound ports, dispatch loop) and client (announce ports, keepalive pings) without removing existing TCP logic.
2. Move voice capture/playback to Opus over UDP with small jitter buffer and concealment for lost packets.
3. Transition screen sharing to H.264 over UDP with keyframe markers and frame dropping under load.
4. Evaluate congestion control and bitrate adaptation (resolution/fps throttling) using observed loss or one-way delay.

## Open questions
- Whether to support NAT traversal beyond simple server-relay (SFU) model.
- How to authenticate UDP flows (e.g., per-login token in header flags) to avoid spoofing.
- Whether the server should ever mix audio or transcode video for bandwidth-constrained receivers.
