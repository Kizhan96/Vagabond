#include "message_protocol.h"
#include <QDataStream>
#include <QIODevice>

QByteArray MessageProtocol::encodeMessage(const Message &message) {
    QByteArray buffer;
    QDataStream out(&buffer, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_DefaultCompiledVersion);

    // Placeholder for frame length.
    out << static_cast<quint32>(0);
    out << static_cast<quint8>(message.type);
    out << message.sender;
    out << message.recipient;
    out << message.payload;
    out << static_cast<qint64>(message.timestampMs);

    // Rewrite frame length (bytes after the length field).
    const quint32 frameLength = static_cast<quint32>(buffer.size() - sizeof(quint32));
    QDataStream patch(&buffer, QIODevice::WriteOnly);
    patch.setVersion(QDataStream::Qt_DefaultCompiledVersion);
    patch << frameLength;

    return buffer;
}

bool MessageProtocol::decodeMessage(const QByteArray &data, Message &message) {
    if (data.size() < static_cast<int>(sizeof(quint32))) {
        return false;
    }

    QDataStream in(data);
    in.setVersion(QDataStream::Qt_DefaultCompiledVersion);

    quint32 frameLength = 0;
    in >> frameLength;
    if (frameLength + sizeof(quint32) != static_cast<quint32>(data.size())) {
        return false;
    }

    quint8 type = 0;
    QString sender;
    QString recipient;
    QByteArray payload;
    qint64 timestampMs = 0;

    in >> type;
    in >> sender;
    in >> recipient;
    in >> payload;
    in >> timestampMs;

    if (in.status() != QDataStream::Ok) {
        return false;
    }

    message.type = static_cast<MessageType>(type);
    message.sender = sender;
    message.recipient = recipient;
    message.payload = payload;
    message.timestampMs = timestampMs;
    return true;
}
