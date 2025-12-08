#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
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
        voiceUdp.bind(QHostAddress::Any, kVoiceUdpPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
        videoUdp.bind(QHostAddress::Any, kVideoUdpPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
        connect(&voiceUdp, &QUdpSocket::readyRead, this, &Server::onVoiceUdpReady);
        connect(&videoUdp, &QUdpSocket::readyRead, this, &Server::onVideoUdpReady);
        bot.start();
    }

private slots:
    void onNewConnection() {
        QTcpSocket *clientSocket = nextPendingConnection();
        sockets.insert(clientSocket);
        qInfo() << "[conn] new connection from" << clientSocket->peerAddress().toString() << clientSocket->peerPort();
        connect(clientSocket, &QTcpSocket::readyRead, this, [this, clientSocket]() { onReadyRead(clientSocket); });
        connect(clientSocket, &QTcpSocket::disconnected, this, [this, clientSocket]() { onDisconnected(clientSocket); });
    }

private:
    void onDisconnected(QTcpSocket *socket) {
        qInfo() << "[conn] disconnected" << socket->peerAddress().toString() << socket->peerPort();
        sockets.remove(socket);
        buffers.remove(socket);
        const QString user = userBySocket.take(socket);
        if (!user.isEmpty()) {
            const auto ep = udpByUser.take(user);
            if (ep.voicePort) voiceEndpointToUser.remove(endpointKey(ep.address, ep.voicePort));
            if (ep.videoPort) videoEndpointToUser.remove(endpointKey(ep.address, ep.videoPort));
        }
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
                qWarning() << "[proto] decode failed, size" << frame.size();
                sendError(socket, "Malformed message");
                continue;
            }
            if (message.type != MessageType::VoiceChunk &&
                message.type != MessageType::ScreenFrame &&
                message.type != MessageType::StreamAudio) {
                qInfo() << "[recv]" << static_cast<int>(message.type)
                        << "from" << socket->peerAddress().toString() << socket->peerPort();
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
        case MessageType::UdpPortsAnnouncement:
            handleUdpPorts(socket, msg);
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
            qInfo() << "[auth] register attempt" << username;
            ok = auth.registerUser(username, password);
            if (!ok) {
                qWarning() << "[auth] register failed, user exists" << username;
                sendError(socket, "User already exists");
                return;
            }
        } else {
            qInfo() << "[auth] login attempt" << username;
            ok = auth.loginUser(username, password);
            if (!ok) {
                qWarning() << "[auth] invalid credentials" << username;
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
        qInfo() << "[auth] success" << username;
        broadcastUsersList();
    }

    void handleUdpPorts(QTcpSocket *socket, const Message &msg) {
        const QString sender = userBySocket.value(socket);
        if (sender.isEmpty()) {
            sendError(socket, "Not authenticated");
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(msg.payload);
        if (!doc.isObject()) {
            sendError(socket, "Invalid UDP announce payload");
            return;
        }
        const QJsonObject obj = doc.object();
        const quint16 voicePort = static_cast<quint16>(obj.value("voicePort").toInt());
        const quint16 videoPort = static_cast<quint16>(obj.value("videoPort").toInt());
        const QHostAddress addr = socket->peerAddress();

        const auto previous = udpByUser.take(sender);
        if (previous.voicePort) voiceEndpointToUser.remove(endpointKey(previous.address, previous.voicePort));
        if (previous.videoPort) videoEndpointToUser.remove(endpointKey(previous.address, previous.videoPort));

        UdpEndpoints ep;
        ep.address = addr;
        ep.voicePort = voicePort;
        ep.videoPort = videoPort;
        udpByUser.insert(sender, ep);
        if (voicePort) voiceEndpointToUser.insert(endpointKey(addr, voicePort), sender);
        if (videoPort) videoEndpointToUser.insert(endpointKey(addr, videoPort), sender);
        qInfo() << "[udp] announce" << sender << addr.toString() << "voice" << voicePort << "video" << videoPort;
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
        qInfo() << "[chat]" << sender << ":" << text;

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
        qInfo() << "[history] sent to" << userBySocket.value(socket);
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
        qInfo() << "[users] list sent, count" << users.size();
    }

    void handleLogout(QTcpSocket *socket) {
        qInfo() << "[auth] logout" << userBySocket.value(socket);
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
        qWarning() << "[error]" << text;
    }

    void onVoiceUdpReady() {
        while (voiceUdp.hasPendingDatagrams()) {
            QByteArray datagram;
            datagram.resize(int(voiceUdp.pendingDatagramSize()));
            QHostAddress addr;
            quint16 port = 0;
            voiceUdp.readDatagram(datagram.data(), datagram.size(), &addr, &port);
            const QString sender = voiceEndpointToUser.value(endpointKey(addr, port));
            if (sender.isEmpty()) continue;
            MediaHeader hdr{};
            QByteArray payload;
            if (!unpackMediaDatagram(datagram, hdr, payload)) continue;
            hdr.ssrc = ssrcForUser(sender);
            const QByteArray outbound = packMediaDatagram(hdr, payload);
            for (auto it = udpByUser.cbegin(); it != udpByUser.cend(); ++it) {
                if (it.key() == sender) continue;
                if (it.value().voicePort == 0) continue;
                voiceUdp.writeDatagram(outbound, it.value().address, it.value().voicePort);
            }
        }
    }

    void onVideoUdpReady() {
        while (videoUdp.hasPendingDatagrams()) {
            QByteArray datagram;
            datagram.resize(int(videoUdp.pendingDatagramSize()));
            QHostAddress addr;
            quint16 port = 0;
            videoUdp.readDatagram(datagram.data(), datagram.size(), &addr, &port);
            const QString sender = videoEndpointToUser.value(endpointKey(addr, port));
            if (sender.isEmpty()) continue;
            MediaHeader hdr{};
            QByteArray payload;
            if (!unpackMediaDatagram(datagram, hdr, payload)) continue;
            hdr.ssrc = ssrcForUser(sender);
            const QByteArray outbound = packMediaDatagram(hdr, payload);
            for (auto it = udpByUser.cbegin(); it != udpByUser.cend(); ++it) {
                if (it.key() == sender) continue;
                if (it.value().videoPort == 0) continue;
                videoUdp.writeDatagram(outbound, it.value().address, it.value().videoPort);
            }
        }
    }

    QString endpointKey(const QHostAddress &addr, quint16 port) const {
        return QStringLiteral("%1:%2").arg(addr.toString()).arg(port);
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
    struct UdpEndpoints {
        QHostAddress address;
        quint16 voicePort = 0;
        quint16 videoPort = 0;
    };
    QHash<QString, UdpEndpoints> udpByUser;
    QHash<QString, QString> voiceEndpointToUser;
    QHash<QString, QString> videoEndpointToUser;
    QUdpSocket voiceUdp;
    QUdpSocket videoUdp;
    static constexpr quint16 kVoiceUdpPort = 40000;
    static constexpr quint16 kVideoUdpPort = 40001;
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
