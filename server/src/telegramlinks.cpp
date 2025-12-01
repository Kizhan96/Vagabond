#include "telegramlinks.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

TelegramLinks::TelegramLinks(const QString &linksFilePath, QObject *parent)
    : QObject(parent), m_linksFilePath(linksFilePath) {
    load();
}

bool TelegramLinks::hasAccount(qint64 telegramId) const {
    return m_links.contains(telegramId);
}

bool TelegramLinks::getUsername(qint64 telegramId, QString *outUsername) const {
    if (!m_links.contains(telegramId)) return false;
    if (outUsername) *outUsername = m_links.value(telegramId);
    return true;
}

bool TelegramLinks::link(qint64 telegramId, const QString &username, QString *outError) {
    if (m_links.contains(telegramId)) {
        if (outError) *outError = QStringLiteral("telegram id already linked");
        return false;
    }
    m_links[telegramId] = username;
    save();
    return true;
}

void TelegramLinks::load() {
    QFile file(m_linksFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_links.clear();
        return;
    }
    const QByteArray data = file.readAll();
    file.close();

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    const QJsonObject obj = doc.object();
    m_links.clear();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        bool ok = false;
        const qint64 tgId = it.key().toLongLong(&ok);
        if (!ok) continue;
        m_links[tgId] = it.value().toString();
    }
}

void TelegramLinks::save() {
    QJsonObject obj;
    for (auto it = m_links.begin(); it != m_links.end(); ++it) {
        obj[QString::number(it.key())] = it.value();
    }
    QJsonDocument doc(obj);
    QFile file(m_linksFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
}
