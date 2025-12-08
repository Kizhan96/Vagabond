#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <QHash>
#include <QDebug>
#include <QBuffer>
#include <QUrl>
#include <QUrlQuery>
#include "authentication.h"
#include "chat.h"
#include "message_protocol.h"
#include "history.h"
#include "telegrambot.h"
#include "telegramlinks.h"

class HttpBridge : public QObject {
    Q_OBJECT

public:
    explicit HttpBridge(QObject *parent = nullptr) : QObject(parent) {
        connect(&httpServer, &QTcpServer::newConnection, this, &HttpBridge::onNewConnection);
    }

    bool start(quint16 port) {
        if (!httpServer.listen(QHostAddress::Any, port)) {
            qWarning() << "[http] bridge listen failed" << port << httpServer.errorString();
            return false;
        }
        qInfo() << "[http] bridge ready on" << port << "(/mjpeg/<user>, /audio/<user>, /viewer?user=...)";
        return true;
    }

    void updateFrame(const QString &user, const QByteArray &jpeg) {
        if (user.isEmpty() || jpeg.isEmpty()) return;
        latestJpeg[user] = jpeg;
        const auto watchers = mjpegByUser.values(user);
        for (QTcpSocket *sock : watchers) {
            sendJpeg(sock, jpeg);
        }
    }

    void pushAudio(const QString &user, const QByteArray &pcm) {
        if (user.isEmpty() || pcm.isEmpty()) return;
        const auto listeners = audioByUser.values(user);
        for (QTcpSocket *sock : listeners) {
            sendChunk(sock, pcm);
        }
    }

private slots:
    void onNewConnection() {
        while (httpServer.hasPendingConnections()) {
            QTcpSocket *sock = httpServer.nextPendingConnection();
            connect(sock, &QTcpSocket::readyRead, this, [this, sock]() { onSocketReady(sock); });
            connect(sock, &QTcpSocket::disconnected, this, [this, sock]() { cleanupSocket(sock); });
        }
    }

    void onSocketReady(QTcpSocket *sock) {
        if (!sock) return;
        Client &client = clients[sock];
        client.buffer.append(sock->readAll());
        if (client.headersParsed) return;
        const int endIdx = client.buffer.indexOf("\r\n\r\n");
        if (endIdx == -1) return;
        const QList<QByteArray> lines = client.buffer.left(endIdx).split('\n');
        if (lines.isEmpty()) {
            sendSimpleResponse(sock, "400 Bad Request", "Missing request line");
            return;
        }
        const QList<QByteArray> parts = lines.first().trimmed().split(' ');
        if (parts.size() < 2) {
            sendSimpleResponse(sock, "400 Bad Request", "Malformed request line");
            return;
        }
        handlePath(sock, QString::fromUtf8(parts.at(1)));
        client.headersParsed = true;
    }

private:
    enum class ClientKind { Unknown, Html, Mjpeg, Audio };
    struct Client {
        QByteArray buffer;
        QString user;
        ClientKind kind = ClientKind::Unknown;
        bool headersParsed = false;
    };

    QString extractUser(const QString &path, const QString &prefix) const {
        const QString afterPrefix = path.mid(prefix.size());
        if (afterPrefix.startsWith('/')) {
            return afterPrefix.section('/', 1, 1);
        }
        const QUrl url(path);
        QUrlQuery query(url);
        return query.queryItemValue("user");
    }

    void handlePath(QTcpSocket *sock, const QString &path) {
        if (path.startsWith("/mjpeg")) {
            const QString user = extractUser(path, QStringLiteral("/mjpeg"));
            if (user.isEmpty()) {
                sendSimpleResponse(sock, "400 Bad Request", "Provide /mjpeg/<user>");
                return;
            }
            clients[sock].kind = ClientKind::Mjpeg;
            clients[sock].user = user;
            mjpegByUser.insert(user, sock);
            sock->write("HTTP/1.1 200 OK\r\n"
                        "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
                        "Cache-Control: no-cache\r\n"
                        "Connection: keep-alive\r\n\r\n");
            if (latestJpeg.contains(user)) {
                sendJpeg(sock, latestJpeg.value(user));
            }
            return;
        }
        if (path.startsWith("/audio")) {
            const QString user = extractUser(path, QStringLiteral("/audio"));
            if (user.isEmpty()) {
                sendSimpleResponse(sock, "400 Bad Request", "Provide /audio/<user>");
                return;
            }
            clients[sock].kind = ClientKind::Audio;
            clients[sock].user = user;
            audioByUser.insert(user, sock);
            sock->write("HTTP/1.1 200 OK\r\n"
                        "Content-Type: audio/wav\r\n"
                        "Transfer-Encoding: chunked\r\n"
                        "Cache-Control: no-cache\r\n"
                        "Connection: close\r\n\r\n");
            sendChunk(sock, wavHeader());
            return;
        }

        clients[sock].kind = ClientKind::Html;
        const QByteArray body = viewerHtml();
        QByteArray resp;
        resp.append("HTTP/1.1 200 OK\r\n");
        resp.append("Content-Type: text/html; charset=utf-8\r\n");
        resp.append("Content-Length: ");
        resp.append(QByteArray::number(body.size()));
        resp.append("\r\n\r\n");
        resp.append(body);
        sock->write(resp);
        sock->disconnectFromHost();
    }

    QByteArray viewerHtml() const {
        static const QByteArray html =
            "<!doctype html><html><head><meta charset=\"utf-8\"><title>Vagabond Web Viewer</title>"
            "<style>body{margin:0;background:#111;color:#eee;font-family:sans-serif;}#wrap{display:flex;flex-direction:column;height:100vh;}"
            "#video{flex:1;object-fit:contain;background:#000;}header{padding:8px 12px;background:#222;}label{margin-right:6px;}"
            "</style></head><body><div id=\"wrap\"><header><label>User</label><input id=\"u\"/><button id=\"go\">Watch</button></header>"
            "<img id=\"video\"/><audio id=\"audio\" controls autoplay></audio></div><script>const p=new URLSearchParams(location.search);"
            "const u=document.getElementById('u');u.value=p.get('user')||'';const img=document.getElementById('video');const aud=document.getElementById('audio');"
            "function set(){if(!u.value)return;img.src='/mjpeg/'+encodeURIComponent(u.value);aud.src='/audio/'+encodeURIComponent(u.value);}"
            "document.getElementById('go').onclick=set;if(u.value)set();</script></body></html>";
        return html;
    }

    QByteArray wavHeader() const {
        QByteArray header;
        QBuffer buf(&header);
        buf.open(QIODevice::WriteOnly);
        QDataStream ds(&buf);
        ds.setByteOrder(QDataStream::LittleEndian);
        const quint32 dataSize = 0xFFFFFFFF;
        const quint16 channels = 2;
        const quint32 sampleRate = 48000;
        const quint16 bits = 16;
        const quint32 byteRate = sampleRate * channels * bits / 8;
        const quint16 blockAlign = channels * bits / 8;
        ds.writeRawData("RIFF", 4);
        ds << dataSize;
        ds.writeRawData("WAVEfmt ", 8);
        ds << static_cast<quint32>(16);
        ds << static_cast<quint16>(1);
        ds << channels;
        ds << sampleRate;
        ds << byteRate;
        ds << blockAlign;
        ds << bits;
        ds.writeRawData("data", 4);
        ds << dataSize;
        return header;
    }

    void sendJpeg(QTcpSocket *sock, const QByteArray &jpeg) {
        if (!sock || jpeg.isEmpty()) return;
        QByteArray part;
        part.append("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: ");
        part.append(QByteArray::number(jpeg.size()));
        part.append("\r\n\r\n");
        part.append(jpeg);
        part.append("\r\n");
        sock->write(part);
    }

    void sendChunk(QTcpSocket *sock, const QByteArray &payload) {
        if (!sock || payload.isEmpty()) return;
        QByteArray chunk;
        chunk.append(QByteArray::number(payload.size(), 16));
        chunk.append("\r\n");
        chunk.append(payload);
        chunk.append("\r\n");
        sock->write(chunk);
    }

    void sendSimpleResponse(QTcpSocket *sock, const QByteArray &code, const QByteArray &body) {
        QByteArray resp;
        resp.append("HTTP/1.1 ").append(code).append("\r\n");
        resp.append("Content-Type: text/plain\r\n");
        resp.append("Content-Length: ");
        resp.append(QByteArray::number(body.size()));
        resp.append("\r\n\r\n");
        resp.append(body);
        sock->write(resp);
        sock->disconnectFromHost();
        cleanupSocket(sock);
    }

    void cleanupSocket(QTcpSocket *sock) {
        if (!sock) return;
        const Client client = clients.take(sock);
        if (client.kind == ClientKind::Mjpeg && !client.user.isEmpty()) {
            mjpegByUser.remove(client.user, sock);
        }
        if (client.kind == ClientKind::Audio && !client.user.isEmpty()) {
            audioByUser.remove(client.user, sock);
        }
        sock->deleteLater();
    }

    QTcpServer httpServer;
    QHash<QTcpSocket*, Client> clients;
    QMultiHash<QString, QTcpSocket*> mjpegByUser;
    QMultiHash<QString, QTcpSocket*> audioByUser;
    QHash<QString, QByteArray> latestJpeg;
};

class Server : public QTcpServer {
    Q_OBJECT

public:
    Server(QObject *parent = nullptr)
        : QTcpServer(parent),
          httpBridge(this),
          auth("users.json"),
          links("telegram_links.json"),
          bot(qEnvironmentVariable("TG_BOT_TOKEN"), &auth, &links) {
        connect(this, &QTcpServer::newConnection, this, &Server::onNewConnection);
        voiceUdp.bind(QHostAddress::Any, kVoiceUdpPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
        videoUdp.bind(QHostAddress::Any, kVideoUdpPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
        connect(&voiceUdp, &QUdpSocket::readyRead, this, &Server::onVoiceUdpReady);
        connect(&videoUdp, &QUdpSocket::readyRead, this, &Server::onVideoUdpReady);
        httpBridge.start(8080);
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
            for (auto it = activeMedia.begin(); it != activeMedia.end(); ++it) {
                if (it.value().remove(user)) {
                    broadcastMediaUpdate(it.key(), user, "stop", socket);
                }
            }
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
                message.type != MessageType::StreamAudio &&
                message.type != MessageType::WebFrame) {
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
        case MessageType::ChatMedia:
            handleChatMedia(socket, msg);
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
        case MessageType::WebFrame:
            handleWebFrame(socket, msg);
            break;
        case MessageType::MediaControl:
            handleMediaControl(socket, msg);
            break;
        case MessageType::Ping:
            handlePing(socket);
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
        sendMediaSnapshot(socket);
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

    void handleChatMedia(QTcpSocket *socket, const Message &msg) {
        const QString sender = userBySocket.value(socket);
        if (sender.isEmpty()) {
            sendError(socket, "Not authenticated");
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(msg.payload);
        if (!doc.isObject()) {
            sendError(socket, "Invalid media payload");
            return;
        }
        const QJsonObject obj = doc.object();
        const QString mime = obj.value("mime").toString();
        const QString caption = obj.value("text").toString();
        const QByteArray data = QByteArray::fromBase64(obj.value("dataBase64").toString().toUtf8());
        if (mime.isEmpty() || data.isEmpty()) {
            sendError(socket, "Media requires mime and dataBase64");
            return;
        }

        Message outbound = msg;
        outbound.sender = sender;
        outbound.timestampMs = QDateTime::currentMSecsSinceEpoch();
        const QByteArray encoded = MessageProtocol::encodeMessage(outbound);
        for (QTcpSocket *sock : sockets) {
            if (sock && sock->state() == QAbstractSocket::ConnectedState) {
                sock->write(encoded);
            }
        }

        const QString summary = QStringLiteral("%1 отправил медиа (%2, %3 bytes)%4")
                                    .arg(sender, mime)
                                    .arg(data.size())
                                    .arg(caption.isEmpty() ? QString() : QStringLiteral(": %1").arg(caption));
        history.saveMessage(summary);
        qInfo() << "[media]" << summary;
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
        if (msg.payload.size() > static_cast<int>(sizeof(quint32) + sizeof(qint64))) {
            QByteArray pcm = msg.payload.mid(sizeof(quint32) + sizeof(qint64));
            httpBridge.pushAudio(sender, pcm);
        }
    }

    void handleWebFrame(QTcpSocket *socket, const Message &msg) {
        const QString sender = userBySocket.value(socket);
        if (sender.isEmpty()) {
            sendError(socket, "Not authenticated");
            return;
        }
        httpBridge.updateFrame(sender, msg.payload);
    }

    void handleMediaControl(QTcpSocket *socket, const Message &msg) {
        const QString sender = userBySocket.value(socket);
        if (sender.isEmpty()) {
            sendError(socket, "Not authenticated");
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(msg.payload);
        if (!doc.isObject()) {
            sendError(socket, "Invalid media control payload");
            return;
        }
        const QJsonObject obj = doc.object();
        const QString kind = obj.value("kind").toString();
        const QString state = obj.value("state").toString();
        if (kind.isEmpty() || (state != "start" && state != "stop")) {
            sendError(socket, "Media control requires kind/state");
            return;
        }

        if (state == "start") {
            activeMedia[kind].insert(sender);
        } else {
            activeMedia[kind].remove(sender);
        }

        broadcastMediaUpdate(kind, sender, state, socket);
        qInfo() << "[media]" << sender << kind << state;
    }

    void handlePing(QTcpSocket *socket) {
        Message resp;
        resp.type = MessageType::Pong;
        resp.sender = "server";
        resp.timestampMs = QDateTime::currentMSecsSinceEpoch();
        resp.payload = QByteArrayLiteral("pong");
        socket->write(MessageProtocol::encodeMessage(resp));
    }

    void broadcastMediaUpdate(const QString &kind, const QString &user, const QString &state, QTcpSocket *exclude = nullptr) {
        QJsonObject announcement;
        announcement.insert("kind", kind);
        announcement.insert("state", state);
        announcement.insert("from", user);

        Message outbound;
        outbound.type = MessageType::MediaControl;
        outbound.sender = user;
        outbound.timestampMs = QDateTime::currentMSecsSinceEpoch();
        outbound.payload = QJsonDocument(announcement).toJson(QJsonDocument::Compact);

        const QByteArray encoded = MessageProtocol::encodeMessage(outbound);
        for (QTcpSocket *sock : sockets) {
            if (sock && sock->state() == QAbstractSocket::ConnectedState && sock != exclude) {
                sock->write(encoded);
            }
        }
    }

    void sendMediaSnapshot(QTcpSocket *socket) {
        QJsonArray active;
        for (auto it = activeMedia.cbegin(); it != activeMedia.cend(); ++it) {
            for (const QString &user : it.value()) {
                QJsonObject entry;
                entry.insert("kind", it.key());
                entry.insert("state", "start");
                entry.insert("from", user);
                active.append(entry);
            }
        }

        if (active.isEmpty()) return;

        QJsonObject payload;
        payload.insert("snapshot", true);
        payload.insert("active", active);

        Message resp;
        resp.type = MessageType::MediaControl;
        resp.sender = "server";
        resp.timestampMs = QDateTime::currentMSecsSinceEpoch();
        resp.payload = QJsonDocument(payload).toJson(QJsonDocument::Compact);
        socket->write(MessageProtocol::encodeMessage(resp));
    }

    void handleMediaControl(QTcpSocket *socket, const Message &msg) {
        const QString sender = userBySocket.value(socket);
        if (sender.isEmpty()) {
            sendError(socket, "Not authenticated");
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(msg.payload);
        if (!doc.isObject()) {
            sendError(socket, "Invalid media control payload");
            return;
        }
        const QJsonObject obj = doc.object();
        const QString kind = obj.value("kind").toString();
        const QString state = obj.value("state").toString();
        if (kind.isEmpty() || (state != "start" && state != "stop")) {
            sendError(socket, "Media control requires kind/state");
            return;
        }

        if (state == "start") {
            activeMedia[kind].insert(sender);
        } else {
            activeMedia[kind].remove(sender);
        }

        broadcastMediaUpdate(kind, sender, state, socket);
        qInfo() << "[media]" << sender << kind << state;
    }

    void handlePing(QTcpSocket *socket) {
        Message resp;
        resp.type = MessageType::Pong;
        resp.sender = "server";
        resp.timestampMs = QDateTime::currentMSecsSinceEpoch();
        resp.payload = QByteArrayLiteral("pong");
        socket->write(MessageProtocol::encodeMessage(resp));
    }

    void broadcastMediaUpdate(const QString &kind, const QString &user, const QString &state, QTcpSocket *exclude = nullptr) {
        QJsonObject announcement;
        announcement.insert("kind", kind);
        announcement.insert("state", state);
        announcement.insert("from", user);

        Message outbound;
        outbound.type = MessageType::MediaControl;
        outbound.sender = user;
        outbound.timestampMs = QDateTime::currentMSecsSinceEpoch();
        outbound.payload = QJsonDocument(announcement).toJson(QJsonDocument::Compact);

        const QByteArray encoded = MessageProtocol::encodeMessage(outbound);
        for (QTcpSocket *sock : sockets) {
            if (sock && sock->state() == QAbstractSocket::ConnectedState && sock != exclude) {
                sock->write(encoded);
            }
        }
    }

    void sendMediaSnapshot(QTcpSocket *socket) {
        QJsonArray active;
        for (auto it = activeMedia.cbegin(); it != activeMedia.cend(); ++it) {
            for (const QString &user : it.value()) {
                QJsonObject entry;
                entry.insert("kind", it.key());
                entry.insert("state", "start");
                entry.insert("from", user);
                active.append(entry);
            }
        }

        if (active.isEmpty()) return;

        QJsonObject payload;
        payload.insert("snapshot", true);
        payload.insert("active", active);

        Message resp;
        resp.type = MessageType::MediaControl;
        resp.sender = "server";
        resp.timestampMs = QDateTime::currentMSecsSinceEpoch();
        resp.payload = QJsonDocument(payload).toJson(QJsonDocument::Compact);
        socket->write(MessageProtocol::encodeMessage(resp));
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

    HttpBridge httpBridge;
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
    QHash<QString, QSet<QString>> activeMedia;
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
