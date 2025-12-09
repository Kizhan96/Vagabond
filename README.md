# Vagabond

Vagabond is a Qt 6.9.1 C++ desktop client that speaks directly to LiveKit. Media is handled by the LiveKit JavaScript SDK inside a Qt WebEngine view, including screen sharing via `getDisplayMedia`.

## Features

- **LiveKit-native media**: Join any LiveKit room using your endpoint + JWT token; publish local audio/video and screen share.
- **Discord-style multi-room tabs**: Open multiple rooms at once, each in its own tab with per-room logs and chat (LiveKit data channel).
- **Device control**: Inline mute/unmute plus microphone/camera device selection and one-click screen sharing.

## Project Structure

```
Vagabond
├── client      # Qt desktop client that embeds the LiveKit JS UI (including screen share)
│   ├── src
│   ├── CMakeLists.txt
│   └── README.md
└── README.md
```

## Setup

1. **Clone**
   ```
   git clone <repository-url>
   cd Vagabond
   ```
2. **Build Client**
   ```
   cd client
   mkdir build
   cd build
   cmake ..
   cmake --build .
   ```

## Run

Point the client at your LiveKit Cloud instance or self-hosted LiveKit server.

1. Create a LiveKit access token for the desired room/user (for example via the LiveKit CLI or REST API).
2. Launch the Qt client with the LiveKit endpoint and token provided via environment variables or by pasting into the UI:

   - `LIVEKIT_URL` – e.g., `wss://your-host.livekit.cloud`
   - `LIVEKIT_TOKEN` – JWT produced by your LiveKit API key/secret

Example:
```
set LIVEKIT_URL=wss://your-host.livekit.cloud
set LIVEKIT_TOKEN=eyJhbGciOi...
./client.exe
```

Once connected, open one or more tabs with room labels; each tab joins the LiveKit room, publishes media, and renders remote participants. Use the **Share screen** control in each tab to present your desktop through LiveKit.

## Windows build & deployment tips

- Prefer a **Release** build unless you have the Qt *debug* libraries installed. A debug build searches for `Qt6Widgetsd.dll` and the other `*d.dll` binaries; if you only installed the default release components, rebuild with `-DCMAKE_BUILD_TYPE=Release` to avoid the missing-debug-DLL error.
- If you do need a debug build, install the matching *Debug* Qt libraries for your compiler (e.g., MinGW 64-bit) so the `Qt6* d.dll` files exist under `C:\Qt\6.9.1\mingw_64\bin`.
- After building, run `windeployqt` against the produced executable to stage all Qt dependencies next to the `.exe`:
  ```powershell
  cd client\build
  windeployqt .\client.exe
  ```
  Make sure the Qt version used by `windeployqt` matches the one you compiled with.
- If launching from a terminal, ensure `C:\Qt\6.9.1\mingw_64\bin` (or your actual Qt path) is on `PATH` so Windows can locate the Qt DLLs.

## Documentation

- `client/README.md` – Vagabond client notes.
