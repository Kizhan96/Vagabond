#include "chat.h"
#include "message_protocol.h"
#include "history.h"
#include <QTcpSocket>
#include <QDateTime>
#include <QDataStream>
#include <QIODevice>

Chat::Chat(QObject *parent) : QObject(parent) {
    // Initialize chat history with a default file
    history = new History(QStringLiteral("chat_history.log"), this);
}

void Chat::sendMessage(const QString &message, const QString &recipient) {
    if (!socket) return;

    Message msg;
    msg.type = MessageType::ChatMessage;
    msg.payload = message.toUtf8();
    msg.recipient = recipient;
    msg.timestampMs = QDateTime::currentMSecsSinceEpoch();

    socket->write(MessageProtocol::encodeMessage(msg));
}

void Chat::receiveMessage(const QByteArray &data) {
    Message msg;
    if (!MessageProtocol::decodeMessage(data, msg)) {
        return;
    }

    if (msg.type == MessageType::ChatMessage) {
        const QString text = QString::fromUtf8(msg.payload);
        history->saveMessage(text); // Store message in history
        emit messageReceived(msg.sender, text); // Notify listeners
    }
}

void Chat::setSocket(QTcpSocket *socket) {
    this->socket = socket;
    connect(socket, &QTcpSocket::readyRead, this, &Chat::onReadyRead);
}

void Chat::onReadyRead() {
    if (!socket) return;

    readBuffer.append(socket->readAll());
    while (readBuffer.size() >= static_cast<int>(sizeof(quint32))) {
        QDataStream peek(&readBuffer, QIODevice::ReadOnly);
        peek.setVersion(QDataStream::Qt_DefaultCompiledVersion);
        quint32 frameLength = 0;
        peek >> frameLength;
        const int totalSize = static_cast<int>(frameLength + sizeof(quint32));
        if (readBuffer.size() < totalSize) {
            break; // incomplete frame
        }

        const QByteArray frame = readBuffer.left(totalSize);
        readBuffer.remove(0, totalSize);
        receiveMessage(frame);
    }
}
