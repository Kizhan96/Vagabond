#include "chat.h"
#include <QDateTime>
#include "message_protocol.h"
#include "types.h"

Chat::Chat(QObject *parent) : QObject(parent) {
    // Initialize chat history
    loadChatHistory();
}

void Chat::sendMessage(const QString &message) {
    if (message.isEmpty()) return;

    Message msg;
    msg.type = MessageType::ChatMessage;
    msg.payload = message.toUtf8();
    msg.timestampMs = QDateTime::currentMSecsSinceEpoch();
    msg.sender = senderName;

    emit messageToSend(MessageProtocol::encodeMessage(msg));
    
    // Store the message in history
    storeMessageInHistory(message);
}

void Chat::receiveMessage(const QByteArray &data) {
    Message msg;
    if (!MessageProtocol::decodeMessage(data, msg)) {
        return;
    }

    if (msg.type != MessageType::ChatMessage) {
        return;
    }

    const QString message = QString::fromUtf8(msg.payload);
    const QString timestamp = QDateTime::fromMSecsSinceEpoch(msg.timestampMs).toString(Qt::ISODate);
    emit messageReceived(msg.sender, message, timestamp);
}

void Chat::storeMessageInHistory(const QString &message) {
    // Store the message in history (implementation depends on the history class)
    history.append(message);
}

void Chat::loadChatHistory() {
    // Load chat history from storage (implementation depends on the history class)
    // This is a placeholder for loading logic
}

QStringList Chat::getChatHistory() const {
    return history;
}
