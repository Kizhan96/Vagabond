# Vagabond

Vagabond is a Qt 6.9.1 C++ desktop client that speaks directly to LiveKit. Media is handled by the LiveKit JavaScript SDK inside a Qt WebEngine view, including screen sharing via `getDisplayMedia`. The client authenticates with a simple **login/password** form and exchanges them for a LiveKit access token via your backend (defaults to `https://livekit.vagabovnr.moscow/api/token`).

## Features

- **LiveKit-native media**: Join any LiveKit room using your endpoint + JWT token obtained automatically from your auth server.
- **Discord-style multi-room tabs**: Open multiple rooms at once, each in its own tab with per-room logs and chat (LiveKit data channel).
- **Device & account control**: Login with username/password (defaults to `test`/`test`), start with mic/camera on or off, mute/unmute, pick devices, and start/stop screen sharing like a Discord-style client.

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

Point the client at your LiveKit Cloud instance or self-hosted LiveKit server. The UI now lets you edit the auth URL directly;
by default it talks to `https://livekit.vagabovnr.moscow/api/token` to exchange a `login`/`room` payload (optionally including `password`) for a JSON
response:

```json
{ "token": "<livekit jwt>", "livekitUrl": "wss://livekit.example.com", "roomName": "general" }
```

Override the endpoint with `LIVEKIT_AUTH_URL` if your backend differs. The UI defaults to the test user `test` / `test` and the
`general` room. After signing in, a tab opens automatically, publishes audio/video (based on the "Join with microphone/camera on"
toggles), and renders remote participants. Use the **Share screen** control in each tab to present your desktop through LiveKit.

If your environment blocks the CDN, set the **SDK URL override** field before connecting. The client will try that URL first and then fall back to the
official CDN, unpkg, and finally to URLs derived from your LiveKit server (e.g., `https://<your-host>/livekit-client.min.js` and `/static/livekit-client.min.js`).

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
