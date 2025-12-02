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
class QSlider;

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
    void onMuteToggle();
    void onScreenShareStart();
    void onScreenShareStop();
    void onFrameReady(const QPixmap &frame);
    void onFullscreenToggle();
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
    void startConnection();
    void setLoggedInUi(bool enabled);
    void updateStreamStatus();
    void openSettingsDialog();
    bool openShareDialog(int &fpsOut, QSize &resolutionOut, int &qualityOut);
    void loadPersistentConfig();
    void savePersistentConfig();
    void updateLoginStatus(const QString &text, const QString &color = "red");
    void updateMicButtonState(bool on);
    void updateShareButtonState(bool on);
    void updateMuteButtonState(bool on);

    QLineEdit *messageEdit;
    QPushButton *sendButton;
    QPushButton *micToggleButton;
    QPushButton *muteButton;
    QPushButton *shareToggleButton;
    QPushButton *logoutButton;
    QPushButton *settingsButton;
    QPushButton *loginButton;
    QLineEdit *loginUserEdit;
    QLineEdit *loginPassEdit;
    QLabel *loginStatusLabel;
    QLabel *loginConnectionLabel;
    QLabel *pingLabel;
    QWidget *loginPanel = nullptr;
    QWidget *mainPanel = nullptr;
    QListWidget *userList;
    QLabel *sharePreview;
    QSlider *streamVolumeSlider = nullptr;
    QLabel *streamVolumeLabel = nullptr;
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
    double micVolume = 1.0;
    double outputVolume = 1.0;
    QByteArray inputDeviceId;
    QByteArray outputDeviceId;
    QSet<QString> streamingUsers;
    bool micOn = false;
    bool micMuted = false;
    QString currentStreamUser;
    bool isLocalSharingPreviewVisible = false;

    void updateUserListDisplay();
    QTimer *connectionTimer = nullptr;
    QTimer *pingTimer = nullptr;
    void updatePing();
};

#endif // WINDOW_H
