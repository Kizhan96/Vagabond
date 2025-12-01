#ifndef CHAT_H
#define CHAT_H

#include <QObject>
#include <QString>
#include <QStringList>

class Chat : public QObject {
    Q_OBJECT
public:
    explicit Chat(QObject *parent = nullptr);

    void sendMessage(const QString &message);
    void receiveMessage(const QByteArray &data);
    QStringList getChatHistory() const;
    void setSender(const QString &name) { senderName = name; }

signals:
    void messageToSend(const QByteArray &encoded);
    void messageReceived(const QString &sender, const QString &message, const QString &timestamp);

private:
    void storeMessageInHistory(const QString &message);
    void loadChatHistory();

    QStringList history;
    QString senderName;
};

#endif // CHAT_H
