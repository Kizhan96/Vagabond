#ifndef AUTHENTICATION_H
#define AUTHENTICATION_H

#include <QObject>
#include <QMap>
#include <QString>

class Authentication : public QObject {
    Q_OBJECT
public:
    explicit Authentication(const QString &dataFilePath, QObject *parent = nullptr);

    bool registerUser(const QString &username, const QString &password);
    bool loginUser(const QString &username, const QString &password);

    // Helpers
    bool userExists(const QString &username) const;
    bool createUserWithRandomPassword(const QString &username,
                                      QString *outPassword,
                                      QString *outError = nullptr);
    bool resetPassword(const QString &username,
                       QString *outPassword,
                       QString *outError = nullptr);
    bool changePassword(const QString &username,
                        const QString &newPassword,
                        QString *outError = nullptr);

private:
    void loadUserData();
    void saveUserData();
    QString hashPassword(const QString &password) const;
    QString generatePassword(int length = 12) const;

    QMap<QString, QString> m_userData; // username -> hashed password
    QString m_dataFilePath;
};

#endif // AUTHENTICATION_H
