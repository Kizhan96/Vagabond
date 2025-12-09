# Vagabond Client

This is a Qt desktop shell around the LiveKit JavaScript SDK. Each voice/video room lives inside its own `QWebEngineView` tab so you can keep multiple LiveKit rooms open at once just like Discord channels. All legacy TCP/UDP/FFmpeg code has been removed; media flows only through LiveKit. Authentication now happens through a username/password form that asks your backend for a LiveKit access token.

## Features

- Join any LiveKit room after exchanging a login/room payload for a LiveKit JWT via the configurable auth URL (defaults to `https://livekit.vagabovnr.moscow/api/token`).
- Embedded UI exposes mute/unmute, device switching for mic/camera, one-click screen share, and in-room chat (LiveKit data channel).
- Event log per tab plus a global log showing when you open/close rooms.

## Setup

```
git clone <repository-url>
cd Vagabond/client
mkdir build
cd build
cmake ..
cmake --build .
```

## Run

Point the client at your auth endpoint (defaults to `https://livekit.vagabovnr.moscow/api/token`):

```powershell
$env:LIVEKIT_AUTH_URL="https://livekit.vagabovnr.moscow/api/token"  # optional, defaults to this; UI field wins
./client.exe
```

The UI is pre-filled with `test` / `test` credentials and a `general` room for quick smoke tests.

## Usage

1. Enter the auth URL (if your server differs), login, optional password, and a room label (or keep the defaults). Choose whether to join with microphone and/or camera on.
2. Click **Sign in & join**. The app calls `LIVEKIT_AUTH_URL` with `{ identity, roomName, room, password? }`, then opens a tab using the returned token and LiveKit URL (honoring `livekitUrl` or `url`).
3. In the tab, use the inline controls to mute/unmute audio, pick input devices, or start/stop **screen sharing**. Camera video is optional.
4. Chat with other participants via the text box; messages are sent over LiveKit's data channels. Open additional rooms with new labels; close tabs to disconnect.

If your network blocks the CDN, type a **SDK URL override** before connecting. The loader tries your override first, then a
`livekit-client.min.js` file placed next to the executable, the official CDN, unpkg, and two URLs derived from your LiveKit host
(`https://<host>/livekit-client.min.js` and `/static/livekit-client.min.js`).
