# Vagabond

Vagabond is a Qt 6.9.1 C++ client-server application that provides authentication, real-time voice, chat, message history storage, and a defined message protocol.

## Features

- **Authentication**: Login and registration with basic credential handling.
- **Voice & Chat**: Bidirectional audio and text messaging.
- **History**: Server-side history log with client retrieval.
- **Protocol**: Length-prefixed binary protocol with typed messages.

## Project Structure

```
Vagabond
├── client
│   ├── src
│   ├── CMakeLists.txt
│   └── README.md
├── server
│   ├── src
│   ├── CMakeLists.txt
│   └── README.md
├── protocol
│   ├── protocol.md
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
3. **Build Server**
   ```
   cd ../../server
   mkdir build
   cd build
   cmake ..
   cmake --build .
   ```

## Run

- **Server**: From `server/build`, run the server binary. It listens on `0.0.0.0:12345`, stores users in `users.json`, and history in `history.log` in the working directory.
- **Client**: From `client/build`, run the client binary. Configure connection via environment variables:
  - `APP_HOST` (default `127.0.0.1`)
  - `APP_PORT` (default `12345`)
  - `APP_USER` (default `demo`)
  - `APP_PASS` (default `demo`)
  - `APP_REGISTER` (`1` to register then login, `0` to only login`)

PowerShell example:
```
set APP_HOST=127.0.0.1
set APP_PORT=12345
set APP_USER=alice
set APP_PASS=secret
set APP_REGISTER=1
./client.exe
```

## Documentation

- `client/README.md` – Vagabond client notes.
- `server/README.md` – Vagabond server notes.
- `protocol/protocol.md` – Message protocol.
