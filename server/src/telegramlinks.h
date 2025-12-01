#ifndef TELEGRAMLINKS_H
#define TELEGRAMLINKS_H

#include <QObject>
#include <QMap>

class TelegramLinks : public QObject {
    Q_OBJECT
public:
    explicit TelegramLinks(const QString &linksFilePath, QObject *parent = nullptr);

    bool hasAccount(qint64 telegramId) const;
    bool getUsername(qint64 telegramId, QString *outUsername) const;
    bool link(qint64 telegramId, const QString &username, QString *outError = nullptr);

private:
    void load();
    void save();

    QString m_linksFilePath;
    QMap<qint64, QString> m_links; // telegramId -> username
};

#endif // TELEGRAMLINKS_H
