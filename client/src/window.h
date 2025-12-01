#ifndef WINDOW_H
#define WINDOW_H

#include <QWidget>
#include <QByteArray>
#include <QTcpSocket>

#include "authentication.h"
#include "chat.h"
#include "voice.h"
#include "screen_share.h"
#include <QMap>

class QLineEdit;
class QPushButton;
class QTextEdit;
class QCheckBox;
class QSlider;
class QLabel;
class QListWidget;
class QComboBox;
class QDialog;
class QMenu;
class QTimer;
class QListWidgetItem;

class ChatWindow : public QWidget {
    Q_OBJECT
public:
    explicit ChatWindow(QWidget *parent = nullptr);

private slots:
    void onConnectClicked();
    void onSendClicked();
    void onSocketReadyRead();
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError err);
    void onLoginResult(bool ok);
    void onMessageReceived(const QString &sender, const QString &message, const QString &timestamp);
    void onMicToggle();
    void onScreenShareStart();
    void onScreenShareStop();
    void onFrameReady(const QPixmap &frame);
    void onFullscreenToggle();
    void onStreamSelected(int index);
    void onLogout();
    void onOpenSettings();
    void onShareConfig();
    void onUserContextMenuRequested(const QPoint &pos);
    void updateConnectionStatus();
    void onMicVolume(int value);
    void onOutputVolume(int value);

private:
    void appendLog(const QString &text);
    void sendHistoryRequest();
    void requestUsersList();
    bool showLoginDialog(const QString &error = QString());
    void startConnection();
    void setLoggedInUi(bool enabled);
    void updateStreamStatus();
    void openSettingsDialog();
    bool openShareDialog(int &fpsOut, QSize &resolutionOut, int &qualityOut);

    QLineEdit *messageEdit;
    QPushButton *sendButton;
    QPushButton *micToggleButton;
    QPushButton *shareToggleButton;
    QPushButton *logoutButton;
    QPushButton *settingsButton;
    QPushButton *watchButton;
    QListWidget *userList;
    QComboBox *streamSelector;
    QLabel *sharePreview;
    QLabel *userLabel;
    QLabel *connectionLabel;
    QTextEdit *chatView;

    Authentication auth;
    Chat chat;
    Voice voice;
    ScreenShare screenShare;
    QTcpSocket socket;
    QByteArray buffer;
    bool loggedIn = false;
    QMap<QString, QPixmap> streamFrames;
    QString hostValue;
    quint16 portValue = 12345;
    QString userValue;
    QString passValue;
    bool watchingRemote = false;
    int shareFps = 10;
    QSize shareResolution = QSize(1280, 720);
    int shareQuality = 60;
    QSet<QString> streamingUsers;
    bool micOn = false;

    void updateUserListDisplay();
    QTimer *connectionTimer = nullptr;
};

#endif // WINDOW_H
