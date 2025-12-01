#ifndef HISTORY_H
#define HISTORY_H

#include <QString>
#include <QStringList>

class History {
public:
    explicit History(const QString &filePath = QString());

    void saveMessage(const QString &message);
    QStringList getMessages() const;

private:
    void loadHistory();

    QString m_filePath;
    QStringList m_messages;
};

#endif // HISTORY_H
