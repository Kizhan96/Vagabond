#pragma once

#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPushButton>
#include <QTabWidget>

class LiveKitWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit LiveKitWindow(QWidget *parent = nullptr);

private slots:
    void connectToLiveKit();
    void handleAuthResponse();
    void closeTab(int index);

private:
    void appendLog(const QString &line);
    void setFormEnabled(bool enabled);
    QUrl authEndpoint() const;
    void openRoomTab(const QString &url, const QString &token, const QString &room,
                     bool startWithAudio, bool startWithVideo);

    QLineEdit *authUrlInput {nullptr};
    QLineEdit *usernameInput {nullptr};
    QLineEdit *passwordInput {nullptr};
    QLineEdit *roomInput {nullptr};
    QCheckBox *audioCheck {nullptr};
    QCheckBox *videoCheck {nullptr};
    QPushButton *connectButton {nullptr};
    QLabel *statusLabel {nullptr};
    QLabel *accountLabel {nullptr};
    QTabWidget *tabWidget {nullptr};
    QNetworkAccessManager network;
    QNetworkReply *pendingAuthReply {nullptr};
    QString lastIdentity;
};
