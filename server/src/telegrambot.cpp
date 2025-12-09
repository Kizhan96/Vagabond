#include "telegrambot.h"
#include "authentication.h"
#include "telegramlinks.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QtGlobal>

TelegramBot::TelegramBot(const QString &token,
                         Authentication *auth,
                         TelegramLinks *links,
                         QObject *parent)
    : QObject(parent),
      m_token(token),
      m_apiUrl(QStringLiteral("https://api.telegram.org/bot%1/").arg(token)),
      m_auth(auth),
      m_links(links) {
    connect(&m_pollTimer, &QTimer::timeout, this, &TelegramBot::pollUpdates);
}

void TelegramBot::start() {
    m_pollTimer.start(2000);
}

void TelegramBot::pollUpdates() {
    const QString url = QStringLiteral("%1getUpdates?offset=%2")
                            .arg(m_apiUrl)
                            .arg(m_lastUpdateId + 1);
    QNetworkRequest req{QUrl(url)};
    QNetworkReply *reply = m_net.get(req);
    connect(reply, &QNetworkReply::finished, this, &TelegramBot::onReplyFinished);
}

void TelegramBot::onReplyFinished() {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) return;
    const QByteArray data = reply->readAll();
    reply->deleteLater();

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return;
    const QJsonObject obj = doc.object();
    if (!obj.value("ok").toBool()) return;
    const QJsonArray res = obj.value("result").toArray();
    for (const QJsonValue &v : res) {
        if (!v.isObject()) continue;
        const QJsonObject upd = v.toObject();
        m_lastUpdateId = qMax(m_lastUpdateId, upd.value("update_id").toInteger());
        handleUpdate(upd);
    }
}

void TelegramBot::handleUpdate(const QJsonObject &update) {
    if (update.contains("message") && update.value("message").isObject()) {
        handleMessage(update.value("message").toObject());
    }
}

void TelegramBot::handleMessage(const QJsonObject &message) {
    const QJsonObject chatObj = message.value("chat").toObject();
    const qint64 chatId = chatObj.value("id").toInteger();
    const QJsonObject fromObj = message.value("from").toObject();
    const qint64 telegramId = fromObj.value("id").toInteger();
    const QString text = message.value("text").toString();

    if (text == "/start") {
        sendMessage(chatId, "Команды: /register, /reset, /createtest [кол-во]");
        return;
    }

    if (text.startsWith("/createtest")) {
        const QStringList parts = text.split(' ', Qt::SkipEmptyParts);
        int count = 5;
        if (parts.size() >= 2) {
            bool ok = false;
            const int parsed = parts.at(1).toInt(&ok);
            if (!ok || parsed <= 0) {
                sendMessage(chatId, "Введите положительное число: /createtest 5");
                return;
            }
            count = parsed;
        }
        count = qBound(1, count, 50); // защитный лимит
        createTestUsers(chatId, count);
        return;
    }

    if (text == "/register") {
        if (m_links->hasAccount(telegramId)) {
            QString uname;
            m_links->getUsername(telegramId, &uname);
            sendMessage(chatId, QStringLiteral("У вас уже есть аккаунт: %1. Для смены пароля используйте /reset.").arg(uname));
            return;
        }
        askForUsername(chatId);
        m_pending[chatId] = PendingReg{telegramId};
        return;
    }

    if (text == "/reset") {
        if (!m_links->hasAccount(telegramId)) {
            sendMessage(chatId, "Сначала используйте /register.");
            return;
        }
        QString uname;
        m_links->getUsername(telegramId, &uname);
        QString pwd, err;
        if (!m_auth->resetPassword(uname, &pwd, &err)) {
            sendMessage(chatId, err.isEmpty() ? "Ошибка сброса." : err);
            return;
        }
        sendMessage(chatId, QStringLiteral("Новый пароль для %1: %2").arg(uname, pwd));
        return;
    }

    if (text == "/change") {
        // пометить, что ждём новый пароль
        m_pending[chatId] = PendingReg{telegramId};
        sendMessage(chatId, "Введите новый пароль:");
        return;
    }

    if (m_pending.contains(chatId)) {
        // Если в pending уже есть username (регистрация), обрабатываем как username
        // иначе считаем, что пришёл новый пароль для /change
        const QString trimmed = text.trimmed();
        PendingReg reg = m_pending.value(chatId);
        // Проверяем, привязан ли уже этот telegramId
        if (m_links->hasAccount(reg.telegramId)) {
            QString uname;
            m_links->getUsername(reg.telegramId, &uname);
            QString err;
            if (!m_auth->changePassword(uname, trimmed, &err)) {
                sendMessage(chatId, err.isEmpty() ? "Ошибка смены пароля." : err);
            } else {
                sendMessage(chatId, QStringLiteral("Пароль для %1 обновлён.").arg(uname));
                m_pending.remove(chatId);
            }
            return;
        }
        // Если telegramId ещё не привязан — это ветка регистрации
        processUsername(chatId, telegramId, trimmed);
        return;
    }
}

void TelegramBot::sendMessage(qint64 chatId, const QString &text) {
    QNetworkRequest req{QUrl(m_apiUrl + "sendMessage")};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject payload{
        {"chat_id", chatId},
        {"text", text}
    };
    m_net.post(req, QJsonDocument(payload).toJson(QJsonDocument::Compact));
}

void TelegramBot::askForUsername(qint64 chatId) {
    sendMessage(chatId, "Введите желаемый логин (латиницей, без пробелов):");
}

void TelegramBot::processUsername(qint64 chatId, qint64 telegramId, const QString &username) {
    if (username.isEmpty()) {
        sendMessage(chatId, "Логин не может быть пустым. Введите логин:");
        return;
    }
    if (m_auth->userExists(username)) {
        sendMessage(chatId, "Логин уже существует, введите другой:");
        return;
    }

    QString pwd, err;
    if (!m_auth->createUserWithRandomPassword(username, &pwd, &err)) {
        sendMessage(chatId, err.isEmpty() ? "Ошибка регистрации." : err);
        m_pending.remove(chatId);
        return;
    }

    QString linkErr;
    m_links->link(telegramId, username, &linkErr);
    if (!linkErr.isEmpty()) {
        sendMessage(chatId, linkErr);
    }

    sendMessage(chatId, QStringLiteral("Готово! Логин: %1 Пароль: %2").arg(username, pwd));
    m_pending.remove(chatId);
}

void TelegramBot::createTestUsers(qint64 chatId, int count) {
    QStringList lines;
    int created = 0;
    int suffix = 1;

    while (created < count && suffix < count + 1000) {
        const QString username = QStringLiteral("testuser%1").arg(suffix);
        if (!m_auth->userExists(username)) {
            QString pwd;
            QString err;
            if (m_auth->createUserWithRandomPassword(username, &pwd, &err)) {
                lines << QStringLiteral("%1 / %2").arg(username, pwd);
                ++created;
            } else if (!err.isEmpty()) {
                sendMessage(chatId, QStringLiteral("Ошибка: %1").arg(err));
                return;
            }
        }
        ++suffix;
    }

    if (lines.isEmpty()) {
        sendMessage(chatId, "Не удалось создать тестовых пользователей (все заняты?)");
        return;
    }

    QString response = QStringLiteral("Создано %1 аккаунтов:\n").arg(created);
    response += lines.join('\n');
    sendMessage(chatId, response);
}
