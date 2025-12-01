#ifndef TYPES_H
#define TYPES_H

#include <QByteArray>
#include <QString>

// Unified message kinds shared by client and server.
enum class MessageType : quint8 {
    LoginRequest = 1,
    LoginResponse = 2,
    ChatMessage = 3,
    VoiceChunk = 4,
    LogoutRequest = 5,
    HistoryRequest = 6,
    HistoryResponse = 7,
    UsersListRequest = 8,
    UsersListResponse = 9,
    ScreenFrame = 10,
    Error = 255
};

struct Message {
    MessageType type = MessageType::Error;
    QString sender;
    QString recipient;
    QByteArray payload; // Text encoded as UTF-8 or raw audio bytes.
    qint64 timestampMs = 0;
};

struct User {
    QString username;
    QString password; // In a real application, passwords should be hashed
};

#endif // TYPES_H
