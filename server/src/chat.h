#ifndef CHAT_H
#define CHAT_H

#include <QObject>
#include <QTcpSocket>
#include <QString>
#include <QByteArray>

class History;

class Chat : public QObject {
    Q_OBJECT
public:
    explicit Chat(QObject *parent = nullptr);

    void setSocket(QTcpSocket *socket);
    void sendMessage(const QString &message, const QString &recipient);

signals:
    void messageReceived(const QString &sender, const QString &message);

private slots:
    void onReadyRead();

private:
    void receiveMessage(const QByteArray &data);

    History *history;
    QTcpSocket *socket = nullptr;
    QByteArray readBuffer;
};

#endif // CHAT_H
