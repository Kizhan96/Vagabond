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
#include "types.h"
#include <QMap>
#include <QImage>
#include <QMediaDevices>
#include <QThread>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <QSet>
#include <QHash>

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

class VideoDecodeWorker : public QObject {
    Q_OBJECT
public:
    explicit VideoDecodeWorker(QObject *parent = nullptr);
    ~VideoDecodeWorker() override;

public slots:
    void processPayload(const QString &sender, const QByteArray &payload);
    void reset();

signals:
    void frameReady(const QString &sender, const QImage &image);
    void streamPresence(const QString &sender);
    void streamStopped(const QString &sender);

private:
    struct DecoderState {
        H264Decoder *decoder = nullptr;
        quint32 lastFrameId = 0;
        bool ready = false;
    };
    QHash<QString, DecoderState> states;
    DecoderState &ensureState(const QString &sender);
    void resetSender(const QString &sender);
};

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
    qint64 queueBytes = 0;
    QMutex mutex;
    QWaitCondition cond;
    bool running = true;
    static constexpr qint64 kMaxQueuedBytes = 1 * 1024 * 1024; // 1MB cap to avoid latency buildup
};

class ChatWindow : public QWidget {
    Q_OBJECT
public:
    explicit ChatWindow(QWidget *parent = nullptr);
    ~ChatWindow() override;

private slots:
    void onConnectClicked();
    void onSendClicked();
    void onAttachClicked();
    void onSocketReadyRead();
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError err);
    void onLoginResult(bool ok);
    void onMessageReceived(const QString &sender, const QString &message, const QString &timestamp);
    void onRemoteFrameReady(const QString &sender, const QImage &image);
    void onRemoteStreamPresence(const QString &sender);
    void onRemoteStreamStopped(const QString &sender);
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
    void onStreamVolume(int value);

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
    QAudioDevice chooseLoopbackAudio() const;
    void sendStreamStopSignal(int delayMs = 0);
    QByteArray applyVoiceGain(const QByteArray &pcm, qreal gain, const QAudioFormat &format);
    void handleChatMediaMessage(const Message &msg);
    void handleMediaControlMessage(const Message &msg);
    void dispatchVideoPayload(const QString &sender, const QByteArray &payload);
    void sendMediaState(const QString &kind, const QString &state);
    void sendUdpAnnouncement();
    void handleVoiceUdp();
    void handleVideoUdp();
    void processIncomingVoice(const QString &sender, const QByteArray &pcm, const QAudioFormat &fmt);
    void handleScreenFrameMessage(const QString &sender, const QByteArray &payload);

    QLineEdit *messageEdit;
    QPushButton *sendButton;
    QPushButton *attachButton;
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
    int webFrameStride = 3;
    int webFrameCounter = 0;
    double micVolume = 1.0;
    double outputVolume = 1.0;
    double streamOutputVolume = 1.0;
    QByteArray inputDeviceId;
    QByteArray outputDeviceId;
    QSet<QString> streamingUsers;
    QSet<QString> mutedUsers;
    QMap<QString, qreal> userVoiceGains;
    QHash<quint32, QString> ssrcToUser;
    bool micOn = false;
    bool playbackMuted = false;
    QString currentStreamUser;
    bool isLocalSharingPreviewVisible = false;
    quint32 lastFrameIdSent = 0;
    bool streamConfigSent = false;
    bool streamPresenceSent = false;
    int framesSinceConfig = 0;
    int framesSincePresence = 0;
    const int configRepeatInterval = 30;
    bool encodingInProgress = false;
    bool hasPendingFrame = false;
    QImage pendingFrame;
    quint32 pendingFrameId = 0;
    qint64 pendingFrameTimestamp = 0;
    H264Encoder h264Encoder;
    FrameEncodeWorker *encoderWorker = nullptr;
    QThread encoderThread;
    quint16 voiceSeq = 0;
    quint16 videoSeq = 0;

    SendWorker *sendWorker = nullptr;
    QThread sendThread;
    VideoDecodeWorker *videoWorker = nullptr;
    QThread videoThread;
    void updateUserListDisplay();
    QTimer *connectionTimer = nullptr;
    QTimer *pingTimer = nullptr;
    void updatePing();
    qint64 lastPingSentMs = 0;
    bool awaitingPong = false;

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
