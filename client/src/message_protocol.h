#ifndef MESSAGE_PROTOCOL_H
#define MESSAGE_PROTOCOL_H

#include <QByteArray>
#include "types.h"

class MessageProtocol {
public:
    static QByteArray encodeMessage(const Message &message);
    static bool decodeMessage(const QByteArray &data, Message &message);
};

#endif // MESSAGE_PROTOCOL_H
