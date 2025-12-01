#include "authentication.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>
#include <QDateTime>
#include "message_protocol.h"
#include "types.h"

Authentication::Authentication(QObject *parent) : QObject(parent), socket(nullptr) {}

void Authentication::setSocket(QTcpSocket *sock) {
    socket = sock;
}

void Authentication::login(const QString &username, const QString &password) {
    sendAuthRequest(username, password, /*doRegister*/ false);
}

void Authentication::registerUser(const QString &username, const QString &password) {
    sendAuthRequest(username, password, /*doRegister*/ true);
}

void Authentication::sendAuthRequest(const QString &username, const QString &password, bool doRegister) {
    if (!socket) {
        emit loginResult(false);
        return;
    }
    QJsonObject obj;
    obj["username"] = username;
    obj["password"] = password;
    obj["register"] = doRegister;

    Message msg;
    msg.type = MessageType::LoginRequest;
    msg.payload = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    msg.timestampMs = QDateTime::currentMSecsSinceEpoch();

    pendingUser = username;
    awaitingLogin = true;
    socket->write(MessageProtocol::encodeMessage(msg));
}

bool Authentication::handleMessage(const Message &msg) {
    if (msg.type == MessageType::LoginResponse) {
        awaitingLogin = false;
        const bool ok = msg.payload == QByteArrayLiteral("ok");
        if (ok) {
            currentUser = pendingUser;
        }
        pendingUser.clear();
        emit loginResult(ok);
        return true;
    }
    if (msg.type == MessageType::Error) {
        if (awaitingLogin) {
            awaitingLogin = false;
            pendingUser.clear();
            emit loginResult(false);
            emit loginError(QString::fromUtf8(msg.payload));
            return true;
        }
        return false;
    }
    return false;
}

void Authentication::logout() {
    currentUser.clear();
    pendingUser.clear();
    awaitingLogin = false;
    if (!socket) return;

    Message msg;
    msg.type = MessageType::LogoutRequest;
    msg.timestampMs = QDateTime::currentMSecsSinceEpoch();
    socket->write(MessageProtocol::encodeMessage(msg));
}
