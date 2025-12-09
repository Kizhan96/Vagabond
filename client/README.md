# Vagabond Client

This is a Qt desktop shell around the LiveKit JavaScript SDK. Each voice/video room lives inside its own `QWebEngineView` tab so you can keep multiple LiveKit rooms open at once just like Discord channels. All legacy TCP/UDP/FFmpeg code has been removed; media flows only through LiveKit.

## Features

- Join any LiveKit room using your LiveKit URL + access token, with a friendly tab title for the room.
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

Set environment variables for convenience or paste directly into the UI:
- `LIVEKIT_URL` – e.g., `wss://your-host.livekit.cloud`
- `LIVEKIT_TOKEN` – JWT created from your LiveKit API key/secret

PowerShell example:
```
$env:LIVEKIT_URL="wss://your-host.livekit.cloud"
$env:LIVEKIT_TOKEN="eyJhbGciOi..."
./client.exe
```

## Usage

1. Enter the LiveKit URL, token, and a short room label, then click **Open room**. A new tab joins the LiveKit room.
2. Use the inline controls to mute/unmute audio, pick input devices, or start/stop **screen sharing** (primary focus). Camera video is optional.
3. Chat with other participants via the text box; messages are sent over LiveKit's data channels.
4. Open additional rooms with new tokens/labels; close tabs to disconnect.
