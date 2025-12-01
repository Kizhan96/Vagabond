#ifndef TELEGRAMBOT_H
#define TELEGRAMBOT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QHash>
#include <QJsonObject>

class Authentication;
class TelegramLinks;

class TelegramBot : public QObject {
    Q_OBJECT
public:
    TelegramBot(const QString &token,
                Authentication *auth,
                TelegramLinks *links,
                QObject *parent = nullptr);

    void start();

private slots:
    void pollUpdates();
    void onReplyFinished();

private:
    void handleUpdate(const QJsonObject &update);
    void handleMessage(const QJsonObject &message);
    void sendMessage(qint64 chatId, const QString &text);
    void askForUsername(qint64 chatId);
    void processUsername(qint64 chatId, qint64 telegramId, const QString &username);

    QString m_token;
    QString m_apiUrl;
    Authentication *m_auth;
    TelegramLinks *m_links;

    QNetworkAccessManager m_net;
    QTimer m_pollTimer;
    qint64 m_lastUpdateId = 0;

    struct PendingReg {
        qint64 telegramId;
    };
    QHash<qint64, PendingReg> m_pending; // key: chatId
};

#endif // TELEGRAMBOT_H
