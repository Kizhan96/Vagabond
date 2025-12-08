#ifndef WINDOW_H
#define WINDOW_H

#include <QWidget>
#include <QByteArray>
#include <QTcpSocket>
#include <QUdpSocket>

#include "authentication.h"
#include "chat.h"
#include "voice.h"
#include "screen_share.h"
#include "video_view.h"
#include "frame_encode_worker.h"
#include "h264_encoder.h"
#include "h264_decoder.h"
#include <QMap>
#include <QImage>
#include <QMediaDevices>
#include <QThread>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <QSet>

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
class QAudioSource;
class QAudioSink;
class QIODevice;

class SendWorker : public QObject {
    Q_OBJECT
public:
    explicit SendWorker(QTcpSocket *socket, QObject *parent = nullptr);
    ~SendWorker() override = default;

public slots:
    void enqueue(const QByteArray &packet);
    void enqueuePriority(const QByteArray &packet);
    void processLoop();
    void stop();

private:
    QTcpSocket *socket = nullptr;
    QQueue<QByteArray> queue;
    QMutex mutex;
    QWaitCondition cond;
    bool running = true;
};

class ChatWindow : public QWidget {
    Q_OBJECT
public:
    explicit ChatWindow(QWidget *parent = nullptr);
    ~ChatWindow() override;

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
    void onEncodedFrame(const QByteArray &data, quint32 frameId, qint64 timestampMs);
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
    void handleAudioDevicesChanged(bool inputsChanged);
    void scrollChatToBottom();
    int calcShareBitrate() const;
    void startStreamAudioCapture();
    void stopStreamAudioCapture();
    void ensureStreamAudioOutput();
    void sendStreamStopSignal(int delayMs = 0);
    QByteArray applyVoiceGain(const QByteArray &pcm, qreal gain, const QAudioFormat &format);
    void sendUdpAnnouncement();
    void handleVoiceUdp();
    void handleVideoUdp();
    void processIncomingVoice(const QString &sender, const QByteArray &pcm, const QAudioFormat &fmt);
    void handleScreenFrameMessage(const QString &sender, const QByteArray &payload);

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
    VideoView *sharePreview;
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
    QUdpSocket voiceUdp;
    QUdpSocket videoUdp;
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
    QSet<QString> mutedUsers;
    QMap<QString, qreal> userVoiceGains;
    QHash<quint32, QString> ssrcToUser;
    bool micOn = false;
    bool micMuted = false;
    QString currentStreamUser;
    bool isLocalSharingPreviewVisible = false;
    qint64 lastFrameTimestamp = 0;
    quint32 lastFrameIdSent = 0;
    QMap<QString, quint32> lastFrameIdReceived;
    bool streamConfigSent = false;
    bool streamPresenceSent = false;
    int framesSinceConfig = 0;
    int framesSincePresence = 0;
    const int configRepeatInterval = 30;
    bool decoderReady = false;
    bool encodingInProgress = false;
    bool hasPendingFrame = false;
    QImage pendingFrame;
    quint32 pendingFrameId = 0;
    qint64 pendingFrameTimestamp = 0;
    H264Encoder h264Encoder;
    H264Decoder h264Decoder;
    FrameEncodeWorker *encoderWorker = nullptr;
    QThread encoderThread;
    quint16 voiceSeq = 0;
    quint16 videoSeq = 0;

    SendWorker *sendWorker = nullptr;
    QThread sendThread;
    void updateUserListDisplay();
    QTimer *connectionTimer = nullptr;
    QTimer *pingTimer = nullptr;
    void updatePing();

    QMediaDevices mediaDevices;
    // Stream audio (system/loopback) mini-RTP-like
    QAudioSource *streamAudioInput = nullptr;
    QIODevice *streamAudioInputDevice = nullptr;
    QAudioSink *streamAudioOutput = nullptr;
    QIODevice *streamAudioOutputDevice = nullptr;
    quint32 streamAudioSeq = 0;
    static constexpr quint16 kVoiceUdpPort = 40000;
    static constexpr quint16 kVideoUdpPort = 40001;
};

#endif // WINDOW_H
