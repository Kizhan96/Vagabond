# Vagabond Client

This is the client component of Vagabond. It handles authentication, chat, voice, history retrieval, screen sharing, and media playback/capture using Qt.

## Features

- Authentication with optional auto-registration (`APP_REGISTER`).
- Real-time chat and voice.
- Screen sharing with H.264 encode/decode pipeline.
- User list and history retrieval.

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

Set environment variables if you need non-default connection settings:
- `APP_HOST` (default `127.0.0.1`)
- `APP_PORT` (default `12345`)
- `APP_USER` (default `demo`)
- `APP_PASS` (default `demo`)
- `APP_REGISTER` (`1` to register then login, `0` to only login)

PowerShell example:
```
$env:APP_USER="alice"
$env:APP_PASS="secret"
$env:APP_REGISTER="1"
./client.exe
```

## Usage

- Log in, then send messages or start voice.
- Use the screen-share toggle to publish your desktop; recipients see the stream in the user list panel.
- Adjust mic/output levels in settings if devices change.
