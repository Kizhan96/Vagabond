#ifndef AUTHENTICATION_H
#define AUTHENTICATION_H

#include <QObject>
#include <QString>

class QTcpSocket;
struct Message;

class Authentication : public QObject {
    Q_OBJECT
public:
    explicit Authentication(QObject *parent = nullptr);

    void setSocket(QTcpSocket *sock);
    void login(const QString &username, const QString &password);
    void registerUser(const QString &username, const QString &password);
    void logout();
    bool handleMessage(const Message &msg);

    QString currentUsername() const { return currentUser; }

signals:
    void loginResult(bool success);
    void loginError(const QString &message);

private:
    void sendAuthRequest(const QString &username, const QString &password, bool doRegister);

    QTcpSocket *socket;
    QString pendingUser;
    QString currentUser;
    bool awaitingLogin = false;
};

#endif // AUTHENTICATION_H
