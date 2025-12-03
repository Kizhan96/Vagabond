#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QHash>
#include <QDebug>
#include "authentication.h"
#include "chat.h"
#include "message_protocol.h"
#include "history.h"
#include "telegrambot.h"
#include "telegramlinks.h"

class Server : public QTcpServer {
    Q_OBJECT

public:
    Server(QObject *parent = nullptr)
        : QTcpServer(parent),
          auth("users.json"),
          links("telegram_links.json"),
          bot(qEnvironmentVariable("TG_BOT_TOKEN"), &auth, &links) {
        connect(this, &QTcpServer::newConnection, this, &Server::onNewConnection);
        bot.start();
    }

private slots:
    void onNewConnection() {
        QTcpSocket *clientSocket = nextPendingConnection();
        sockets.insert(clientSocket);
        connect(clientSocket, &QTcpSocket::readyRead, this, [this, clientSocket]() { onReadyRead(clientSocket); });
        connect(clientSocket, &QTcpSocket::disconnected, this, [this, clientSocket]() { onDisconnected(clientSocket); });
    }

private:
    void onDisconnected(QTcpSocket *socket) {
        sockets.remove(socket);
        buffers.remove(socket);
        userBySocket.remove(socket);
        broadcastUsersList();
        socket->deleteLater();
    }

    void onReadyRead(QTcpSocket *socket) {
        QByteArray &buffer = buffers[socket];
        buffer.append(socket->readAll());

        while (buffer.size() >= static_cast<int>(sizeof(quint32))) {
            QDataStream peek(&buffer, QIODevice::ReadOnly);
            peek.setVersion(QDataStream::Qt_DefaultCompiledVersion);
            quint32 frameLength = 0;
            peek >> frameLength;
            const int totalSize = static_cast<int>(frameLength + sizeof(quint32));
            if (buffer.size() < totalSize) {
                break; // incomplete frame
            }

            const QByteArray frame = buffer.left(totalSize);
            buffer.remove(0, totalSize);

            Message message;
            if (!MessageProtocol::decodeMessage(frame, message)) {
                sendError(socket, "Malformed message");
                continue;
            }
            handleMessage(socket, message);
        }
    }

    void handleMessage(QTcpSocket *socket, const Message &msg) {
        switch (msg.type) {
        case MessageType::LoginRequest:
            handleLogin(socket, msg);
            break;
        case MessageType::ChatMessage:
            handleChat(socket, msg);
            break;
        case MessageType::HistoryRequest:
            handleHistory(socket);
            break;
        case MessageType::UsersListRequest:
            handleUsersList(socket);
            break;
        case MessageType::VoiceChunk:
            handleVoice(socket, msg);
            break;
        case MessageType::ScreenFrame:
            handleScreenFrame(socket, msg);
            break;
        case MessageType::StreamAudio:
            handleStreamAudio(socket, msg);
            break;
        case MessageType::LogoutRequest:
            handleLogout(socket);
            break;
        default:
            sendError(socket, "Unsupported message type");
            break;
        }
    }

    void handleLogin(QTcpSocket *socket, const Message &msg) {
        const QJsonDocument doc = QJsonDocument::fromJson(msg.payload);
        if (!doc.isObject()) {
            sendError(socket, "Invalid login payload");
            return;
        }
        const QJsonObject obj = doc.object();
        const QString username = obj.value("username").toString();
        const QString password = obj.value("password").toString();
        const bool doRegister = obj.value("register").toBool(false);
        if (username.isEmpty() || password.isEmpty()) {
            sendError(socket, "Username/password required");
            return;
        }

        bool ok = false;
        if (doRegister) {
            ok = auth.registerUser(username, password);
            if (!ok) {
                sendError(socket, "User already exists");
                return;
            }
        } else {
            ok = auth.loginUser(username, password);
            if (!ok) {
                sendError(socket, "Invalid credentials");
                return;
            }
        }

        userBySocket[socket] = username;

        Message resp;
        resp.type = MessageType::LoginResponse;
        resp.sender = "server";
        resp.payload = QByteArrayLiteral("ok");
        resp.timestampMs = QDateTime::currentMSecsSinceEpoch();
        socket->write(MessageProtocol::encodeMessage(resp));
        broadcastUsersList();
    }

    void handleChat(QTcpSocket *socket, const Message &msg) {
        const QString sender = userBySocket.value(socket);
        if (sender.isEmpty()) {
            sendError(socket, "Not authenticated");
            return;
        }

        Message outbound = msg;
        outbound.sender = sender;
        outbound.timestampMs = QDateTime::currentMSecsSinceEpoch();

        const QString text = QString::fromUtf8(outbound.payload);
        history.saveMessage(QStringLiteral("%1: %2").arg(sender, text));

        const QByteArray encoded = MessageProtocol::encodeMessage(outbound);
        for (QTcpSocket *sock : sockets) {
            if (sock && sock->state() == QAbstractSocket::ConnectedState) {
                sock->write(encoded);
            }
        }
    }

    void handleHistory(QTcpSocket *socket) {
        Message resp;
        resp.type = MessageType::HistoryResponse;
        resp.sender = "server";
        resp.timestampMs = QDateTime::currentMSecsSinceEpoch();
        resp.payload = history.getMessages().join("\n").toUtf8();
        socket->write(MessageProtocol::encodeMessage(resp));
    }

    void handleUsersList(QTcpSocket *socket) {
        Message resp;
        resp.type = MessageType::UsersListResponse;
        resp.sender = "server";
        resp.timestampMs = QDateTime::currentMSecsSinceEpoch();
        QStringList users = userBySocket.values();
        users.removeAll(QString()); // drop empty (unauthenticated)
        users.removeDuplicates();
        resp.payload = users.join("\n").toUtf8();
        socket->write(MessageProtocol::encodeMessage(resp));
    }

    void handleLogout(QTcpSocket *socket) {
        userBySocket.remove(socket);
        Message resp;
        resp.type = MessageType::LogoutRequest;
        resp.sender = "server";
        resp.timestampMs = QDateTime::currentMSecsSinceEpoch();
        resp.payload = QByteArrayLiteral("bye");
        socket->write(MessageProtocol::encodeMessage(resp));
        broadcastUsersList();
    }

    void handleVoice(QTcpSocket *socket, const Message &msg) {
        const QString sender = userBySocket.value(socket);
        if (sender.isEmpty()) {
            sendError(socket, "Not authenticated");
            return;
        }
        Message outbound = msg;
        outbound.sender = sender;
        outbound.timestampMs = QDateTime::currentMSecsSinceEpoch();
        const QByteArray encoded = MessageProtocol::encodeMessage(outbound);
        for (QTcpSocket *sock : sockets) {
            if (sock && sock->state() == QAbstractSocket::ConnectedState && sock != socket) {
                sock->write(encoded);
            }
        }
    }

    void handleScreenFrame(QTcpSocket *socket, const Message &msg) {
        const QString sender = userBySocket.value(socket);
        if (sender.isEmpty()) {
            sendError(socket, "Not authenticated");
            return;
        }
        Message outbound = msg;
        outbound.sender = sender;
        outbound.timestampMs = QDateTime::currentMSecsSinceEpoch();
        const QByteArray encoded = MessageProtocol::encodeMessage(outbound);
        for (QTcpSocket *sock : sockets) {
            if (sock && sock->state() == QAbstractSocket::ConnectedState && sock != socket) {
                sock->write(encoded);
            }
        }
    }

    void handleStreamAudio(QTcpSocket *socket, const Message &msg) {
        const QString sender = userBySocket.value(socket);
        if (sender.isEmpty()) {
            sendError(socket, "Not authenticated");
            return;
        }
        Message outbound = msg;
        outbound.sender = sender;
        outbound.timestampMs = QDateTime::currentMSecsSinceEpoch();
        const QByteArray encoded = MessageProtocol::encodeMessage(outbound);
        for (QTcpSocket *sock : sockets) {
            if (sock && sock->state() == QAbstractSocket::ConnectedState && sock != socket) {
                sock->write(encoded);
            }
        }
    }

    void sendError(QTcpSocket *socket, const QString &text) {
        Message resp;
        resp.type = MessageType::Error;
        resp.sender = "server";
        resp.timestampMs = QDateTime::currentMSecsSinceEpoch();
        resp.payload = text.toUtf8();
        socket->write(MessageProtocol::encodeMessage(resp));
    }

    void broadcastUsersList() {
        QStringList users = userBySocket.values();
        users.removeAll(QString());
        users.removeDuplicates();
        Message resp;
        resp.type = MessageType::UsersListResponse;
        resp.sender = "server";
        resp.timestampMs = QDateTime::currentMSecsSinceEpoch();
        resp.payload = users.join("\n").toUtf8();
        const QByteArray encoded = MessageProtocol::encodeMessage(resp);
        for (QTcpSocket *sock : sockets) {
            if (sock && sock->state() == QAbstractSocket::ConnectedState) {
                sock->write(encoded);
            }
        }
    }

    Authentication auth;
    TelegramLinks links;
    TelegramBot bot;
    Chat chat;
    MessageProtocol messageProtocol;
    History history{"history.log"};
    QSet<QTcpSocket*> sockets;
    QHash<QTcpSocket*, QByteArray> buffers;
    QHash<QTcpSocket*, QString> userBySocket;
};

#include "main.moc"

int main(int argc, char *argv[]) {
    QCoreApplication a(argc, argv);
    Server server;
    if (!server.listen(QHostAddress::Any, 12345)) {
        qFatal("Failed to bind to port");
    }
    return a.exec();
}
