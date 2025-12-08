#ifndef TYPES_H
#define TYPES_H

#include <QByteArray>
#include <QDataStream>
#include <QDateTime>
#include <QCryptographicHash>
#include <QHash>
#include <QIODevice>
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
    StreamAudio = 11,
    UdpPortsAnnouncement = 12,
    Error = 255
};

struct Message {
    MessageType type = MessageType::Error;
    QString sender;
    QString recipient;
    QByteArray payload; // Text encoded as UTF-8 or raw audio bytes.
    qint64 timestampMs = 0;
};

struct MediaHeader {
    quint8 version = 1;
    quint8 mediaType = 0; // 0 = voice, 1 = video
    quint8 codec = 0;      // 0 = Opus, 1 = H264
    quint8 flags = 0;      // bit0: keyframe, bit1: marker
    quint32 ssrc = 0;
    quint32 timestampMs = 0;
    quint16 seq = 0;
    quint16 payloadLen = 0;
};

static inline quint32 mediaHeaderSize() {
    return sizeof(quint8) * 4 + sizeof(quint32) * 2 + sizeof(quint16) * 2;
}

static inline QByteArray packMediaDatagram(const MediaHeader &hdr, const QByteArray &payload) {
    QByteArray out;
    out.reserve(mediaHeaderSize() + payload.size());
    QDataStream ds(&out, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);
    ds << hdr.version << hdr.mediaType << hdr.codec << hdr.flags << hdr.ssrc << hdr.timestampMs << hdr.seq
       << static_cast<quint16>(payload.size());
    out.append(payload);
    return out;
}

static inline bool unpackMediaDatagram(const QByteArray &datagram, MediaHeader &hdr, QByteArray &payload) {
    if (datagram.size() < static_cast<int>(mediaHeaderSize())) return false;
    QDataStream ds(datagram);
    ds.setByteOrder(QDataStream::BigEndian);
    ds >> hdr.version >> hdr.mediaType >> hdr.codec >> hdr.flags >> hdr.ssrc >> hdr.timestampMs >> hdr.seq >> hdr.payloadLen;
    if (ds.status() != QDataStream::Ok) return false;
    if (datagram.size() < static_cast<int>(mediaHeaderSize() + hdr.payloadLen)) return false;
    payload = datagram.mid(static_cast<int>(mediaHeaderSize()), hdr.payloadLen);
    return true;
}

static inline quint32 ssrcForUser(const QString &username) {
    // Use a deterministic SSRC so that server and clients agree on the
    // identifier regardless of per-process hash randomization.
    const QByteArray hash = QCryptographicHash::hash(username.toUtf8(), QCryptographicHash::Sha1);
    quint32 value = 0;
    QDataStream ds(hash.left(4));
    ds.setByteOrder(QDataStream::BigEndian);
    ds >> value;
    if (value == 0) value = 1; // reserve 0 as invalid
    return value;
}

#endif // TYPES_H
