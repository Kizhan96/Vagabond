#include "authentication.h"
#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>

Authentication::Authentication(const QString &dataFilePath, QObject *parent)
    : QObject(parent), m_dataFilePath(dataFilePath) {
    loadUserData();
}

bool Authentication::registerUser(const QString &username, const QString &password) {
    if (m_userData.contains(username)) return false;
    m_userData[username] = hashPassword(password);
    saveUserData();
    return true;
}

bool Authentication::loginUser(const QString &username, const QString &password) {
    if (!m_userData.contains(username)) return false;
    return m_userData.value(username) == hashPassword(password);
}

bool Authentication::userExists(const QString &username) const {
    return m_userData.contains(username);
}

QString Authentication::generatePassword(int length) const {
    static const QString chars =
        QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
    QString pwd;
    pwd.reserve(length);
    for (int i = 0; i < length; ++i) {
        int idx = QRandomGenerator::global()->bounded(chars.size());
        pwd.append(chars.at(idx));
    }
    return pwd;
}

bool Authentication::createUserWithRandomPassword(const QString &username,
                                                  QString *outPassword,
                                                  QString *outError) {
    if (m_userData.contains(username)) {
        if (outError) *outError = QStringLiteral("username already exists");
        return false;
    }
    const QString pwd = generatePassword();
    m_userData[username] = hashPassword(pwd);
    saveUserData();
    if (outPassword) *outPassword = pwd;
    return true;
}

bool Authentication::resetPassword(const QString &username,
                                   QString *outPassword,
                                   QString *outError) {
    if (!m_userData.contains(username)) {
        if (outError) *outError = QStringLiteral("user does not exist");
        return false;
    }
    const QString pwd = generatePassword();
    m_userData[username] = hashPassword(pwd);
    saveUserData();
    if (outPassword) *outPassword = pwd;
    return true;
}

bool Authentication::changePassword(const QString &username,
                                    const QString &newPassword,
                                    QString *outError) {
    if (!m_userData.contains(username)) {
        if (outError) *outError = QStringLiteral("user does not exist");
        return false;
    }
    if (newPassword.isEmpty()) {
        if (outError) *outError = QStringLiteral("password cannot be empty");
        return false;
    }
    m_userData[username] = hashPassword(newPassword);
    saveUserData();
    return true;
}

void Authentication::loadUserData() {
    QFile file(m_dataFilePath);
    if (!file.open(QIODevice::ReadOnly)) return;
    const QByteArray data = file.readAll();
    file.close();

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    const QJsonObject obj = doc.object();
    m_userData.clear();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        m_userData[it.key()] = it.value().toString();
    }
}

void Authentication::saveUserData() {
    QJsonObject obj;
    for (auto it = m_userData.begin(); it != m_userData.end(); ++it) {
        obj[it.key()] = it.value();
    }
    QJsonDocument doc(obj);
    QFile file(m_dataFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
}

QString Authentication::hashPassword(const QString &password) const {
    return QString(QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex());
}
