#include "history.h"
#include <QFile>
#include <QTextStream>
#include <QDateTime>

History::History(const QString &filePath) : m_filePath(filePath) {
    loadHistory();
}

void History::loadHistory() {
    QFile file(m_filePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine();
            m_messages.append(line);
        }
        file.close();
    }
}

void History::saveMessage(const QString &message) {
    QFile file(m_filePath);
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out << QDateTime::currentDateTime().toString() << ": " << message << "\n";
        file.close();
    }
    m_messages.append(message);
}

QStringList History::getMessages() const {
    return m_messages;
}