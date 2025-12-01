#ifndef HISTORY_H
#define HISTORY_H

#include <QObject>
#include <QString>
#include <QStringList>

class History : public QObject {
    Q_OBJECT
public:
    explicit History(const QString &filename = QString(), QObject *parent = nullptr);

    void loadHistory();
    void saveMessage(const QString &message);
    QStringList getMessages() const;

private:
    QString m_filename;
    QStringList m_messages;
};

#endif // HISTORY_H
