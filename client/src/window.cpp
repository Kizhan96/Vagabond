#include "window.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QCheckBox>
#include <QLabel>
#include <QDateTime>
#include <QDataStream>
#include <QIODevice>
#include <QDebug>
#include <QSlider>
#include <QDialog>
#include <QComboBox>
#include <QListWidget>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>
#include <QBuffer>
#include <QImageReader>
#include <QImage>
#include <QMessageBox>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QListWidgetItem>
#include <QMenu>
#include <QSettings>
#include <QDir>
#include <QCoreApplication>
#include <QProcess>
#include <QRegularExpression>
#include <QIcon>
#include <QTabWidget>
#include <QAudioFormat>
#include <QAudioSource>
#include <QAudioSink>
#include <algorithm>

#include "message_protocol.h"
#include "types.h"
#include "video_view.h"

ChatWindow::ChatWindow(QWidget *parent) : QWidget(parent) {
    hostValue = "213.171.26.107"; // default server IP
    portValue = 12345;
    userValue = QString();
    passValue = QString();

    const QSize iconSize(20, 20);

    sendButton = new QPushButton(this);
    sendButton->setIcon(QIcon(":/icons/icons/send-message.svg"));
    sendButton->setIconSize(iconSize);
    sendButton->setToolTip("Send message");
    micToggleButton = new QPushButton(this);
    micToggleButton->setIcon(QIcon(":/icons/icons/mic-off.svg"));
    micToggleButton->setIconSize(iconSize);
    micToggleButton->setToolTip("Toggle microphone");
    micToggleButton->setCheckable(true);
    micToggleButton->setFixedSize(32, 32);
    muteButton = new QPushButton(this);
    muteButton->setIcon(QIcon(":/icons/icons/sound.svg"));
    muteButton->setIconSize(iconSize);
    muteButton->setToolTip("Mute / Unmute playback");
    muteButton->setCheckable(true);
    muteButton->setFixedSize(32, 32);
    shareToggleButton = new QPushButton(this);
    shareToggleButton->setIcon(QIcon(":/icons/icons/screen-sharing.svg"));
    shareToggleButton->setIconSize(iconSize);
    shareToggleButton->setToolTip("Start/Stop screen share");
    shareToggleButton->setCheckable(true);
    shareToggleButton->setFixedSize(32, 32);
    logoutButton = new QPushButton(this);
    logoutButton->setIcon(QIcon(":/icons/icons/logout.svg"));
    logoutButton->setIconSize(iconSize);
    logoutButton->setToolTip("Logout");
    settingsButton = new QPushButton(this);
    settingsButton->setIcon(QIcon(":/icons/icons/settings.svg"));
    settingsButton->setIconSize(iconSize);
    settingsButton->setToolTip("Settings");
    settingsButton->setFixedSize(32, 32);

    sendButton->setEnabled(false);
    micToggleButton->setEnabled(false);
    shareToggleButton->setEnabled(false);

    chatView = new QTextEdit(this);
    chatView->setReadOnly(true);
    messageEdit = new QLineEdit(this);
    sharePreview = new VideoView(this);
    sharePreview->setMinimumSize(320, 180);
    sharePreview->setPlaceholder("Screen preview");
    sharePreview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    userList = new QListWidget(this);
    userList->setMinimumWidth(160);
    userList->setContextMenuPolicy(Qt::CustomContextMenu);
    userLabel = new QLabel("User: -", this);
    connectionLabel = new QLabel("Disconnected", this);
    // keep single instance

    loginUserEdit = new QLineEdit(this);
    loginPassEdit = new QLineEdit(this);
    loginPassEdit->setEchoMode(QLineEdit::Password);
    loginButton = new QPushButton("Login", this);
    loginStatusLabel = new QLabel(this);
    loginStatusLabel->setStyleSheet("color:red");
    loginStatusLabel->setVisible(false);
    loginConnectionLabel = new QLabel("Offline", this);
    loginConnectionLabel->setStyleSheet("color:red");
    pingLabel = new QLabel("Ping: n/a", this);
    pingLabel->setVisible(false);
    updateMicButtonState(false);
    updateShareButtonState(false);
    updateMuteButtonState(false);

    // encoder thread
    encoderWorker = new FrameEncodeWorker();
    encoderWorker->moveToThread(&encoderThread);
    connect(&encoderThread, &QThread::finished, encoderWorker, &QObject::deleteLater);
    connect(encoderWorker, &FrameEncodeWorker::encodedFrameReady, this, &ChatWindow::onEncodedFrame, Qt::QueuedConnection);
    encoderThread.start();
    encoderWorker->setEncoder(&h264Encoder);
    h264Encoder.init(shareResolution.width(), shareResolution.height(), shareFps, calcShareBitrate());
    decoderReady = h264Decoder.init();

    screenShare.setTargetSize(shareResolution);
    screenShare.setFps(shareFps);

    auto loginLayout = new QHBoxLayout();
    loginLayout->addWidget(loginConnectionLabel);
    loginLayout->addWidget(new QLabel("User:", this));
    loginLayout->addWidget(loginUserEdit);
    loginLayout->addWidget(new QLabel("Password:", this));
    loginLayout->addWidget(loginPassEdit);
    loginLayout->addWidget(loginButton);
    loginLayout->addWidget(loginStatusLabel);
    auto topLayout = new QHBoxLayout();
    topLayout->addWidget(userLabel);
    topLayout->addWidget(connectionLabel);
    topLayout->addWidget(pingLabel);
    topLayout->addStretch();
    topLayout->addWidget(logoutButton);

    auto controlsLayout = new QHBoxLayout();
    controlsLayout->setContentsMargins(0, 4, 0, 0);
    controlsLayout->setSpacing(6);
    controlsLayout->setAlignment(Qt::AlignCenter);
    controlsLayout->addWidget(shareToggleButton);
    controlsLayout->addWidget(micToggleButton);
    controlsLayout->addWidget(muteButton);
    controlsLayout->addWidget(settingsButton);

    auto bottomLayout = new QHBoxLayout();
    bottomLayout->addWidget(messageEdit);
    bottomLayout->addWidget(sendButton);

    auto mainLayout = new QHBoxLayout();
    userList->setMaximumWidth(150);
    auto leftColumn = new QVBoxLayout();
    leftColumn->addWidget(userList, /*stretch*/1);
    leftColumn->addLayout(controlsLayout);
    mainLayout->addLayout(leftColumn, /*stretch*/1);

    auto rightColumn = new QVBoxLayout();
    sharePreview->setVisible(false);
    streamVolumeLabel = new QLabel("Stream volume", this);
    streamVolumeSlider = new QSlider(Qt::Horizontal, this);
    streamVolumeSlider->setRange(0, 100);
    streamVolumeSlider->setValue(static_cast<int>(outputVolume * 100));
    streamVolumeLabel->setVisible(false);
    streamVolumeSlider->setVisible(false);
    rightColumn->addWidget(sharePreview, /*stretch*/4);
    auto streamVolLayout = new QHBoxLayout();
    streamVolLayout->addWidget(streamVolumeLabel);
    streamVolLayout->addWidget(streamVolumeSlider);
    rightColumn->addLayout(streamVolLayout);
    rightColumn->addWidget(chatView, /*stretch*/2);
    rightColumn->addLayout(bottomLayout);
    mainLayout->addLayout(rightColumn, /*stretch*/3);

    auto outer = new QVBoxLayout();
    outer->addLayout(topLayout);
    outer->addLayout(mainLayout);

    mainPanel = new QWidget(this);
    mainPanel->setLayout(outer);

    auto rootLayout = new QVBoxLayout();
    loginPanel = new QWidget(this);
    loginPanel->setLayout(loginLayout);
    rootLayout->addWidget(loginPanel);
    rootLayout->addWidget(mainPanel);
    setLayout(rootLayout);

    auth.setSocket(&socket);

    connect(sendButton, &QPushButton::clicked, this, &ChatWindow::onSendClicked);
    connect(messageEdit, &QLineEdit::returnPressed, this, &ChatWindow::onSendClicked);
    connect(micToggleButton, &QPushButton::clicked, this, &ChatWindow::onMicToggle);
    connect(shareToggleButton, &QPushButton::clicked, this, &ChatWindow::onShareConfig);
    connect(logoutButton, &QPushButton::clicked, this, &ChatWindow::onLogout);
    connect(settingsButton, &QPushButton::clicked, this, &ChatWindow::onOpenSettings);
    connect(&socket, &QTcpSocket::readyRead, this, &ChatWindow::onSocketReadyRead);
    connect(&socket, &QTcpSocket::connected, this, &ChatWindow::onSocketConnected);
    connect(&socket, &QTcpSocket::disconnected, this, &ChatWindow::onSocketDisconnected);
    connect(&socket, &QTcpSocket::errorOccurred, this, &ChatWindow::onSocketError);
    connect(&auth, &Authentication::loginResult, this, &ChatWindow::onLoginResult);
    connect(&auth, &Authentication::loginError, this, [this](const QString &msg) {
        updateLoginStatus(msg.isEmpty() ? "Login failed" : msg, "red");
        loggedIn = false;
        setLoggedInUi(false);
    });
    connect(&chat, &Chat::messageToSend, &socket, [this](const QByteArray &data) { socket.write(data); });
    connect(&chat, &Chat::messageReceived, this, &ChatWindow::onMessageReceived);
    connect(&screenShare, &ScreenShare::frameReady, this, &ChatWindow::onFrameReady);
    connect(&screenShare, &ScreenShare::started, []() {});
    connect(&screenShare, &ScreenShare::stopped, []() {});
    connect(&voice, &Voice::audioChunkReady, this, [this](const QByteArray &data) {
        if (!loggedIn) return;
        Message msg;
        msg.type = MessageType::VoiceChunk;
        msg.timestampMs = QDateTime::currentMSecsSinceEpoch();
        msg.payload = data;
        socket.write(MessageProtocol::encodeMessage(msg));
    });
    connect(&voice, &Voice::playbackError, this, [this](const QString &msg) {
        QMessageBox::warning(this, "Audio playback", msg);
    });
    connect(streamVolumeSlider, &QSlider::valueChanged, this, [this](int value) {
        onOutputVolume(value);
    });
    connect(muteButton, &QPushButton::clicked, this, &ChatWindow::onMuteToggle);
    connect(&mediaDevices, &QMediaDevices::audioInputsChanged, this, [this]() {
        handleAudioDevicesChanged(true);
    });
    connect(&mediaDevices, &QMediaDevices::audioOutputsChanged, this, [this]() {
        handleAudioDevicesChanged(false);
    });
    voice.setPlaybackEnabled(true);
    // stream audio sink will be created lazily when needed
    loadPersistentConfig();
    loginUserEdit->setText(userValue);
    loginPassEdit->setText(passValue);
    voice.setVolumes(micVolume, outputVolume);
    // Apply saved devices if present
    auto findDeviceById = [](const QList<QAudioDevice> &list, const QByteArray &id) -> QAudioDevice {
        for (const auto &d : list) {
            if (d.id() == id) return d;
        }
        return QAudioDevice();
    };
    if (!inputDeviceId.isEmpty()) {
        QAudioDevice dev = findDeviceById(QMediaDevices::audioInputs(), inputDeviceId);
        if (!dev.isNull()) voice.setInputDevice(dev);
    }
    if (!outputDeviceId.isEmpty()) {
        QAudioDevice dev = findDeviceById(QMediaDevices::audioOutputs(), outputDeviceId);
        if (!dev.isNull()) voice.setOutputDevice(dev);
    }

    connect(userList, &QListWidget::customContextMenuRequested, this, &ChatWindow::onUserContextMenuRequested);
    connect(userList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        if (!item) return;
        const QString text = item->data(Qt::UserRole).toString();
        if (streamingUsers.contains(text)) {
            currentStreamUser = text;
            watchingRemote = true;
            if (streamFrames.contains(text)) {
                sharePreview->setFrame(streamFrames[text]);
                sharePreview->setVisible(true);
            }
            streamVolumeLabel->setVisible(true);
            streamVolumeSlider->setVisible(true);
        }
    });

    setWindowTitle("Qt Chat Client");
    resize(1100, 700);
    mainPanel->setVisible(false);
    setLoggedInUi(false);
    show(); // главное окно видно сразу

    connectionTimer = new QTimer(this);
    connect(connectionTimer, &QTimer::timeout, this, &ChatWindow::updateConnectionStatus);
    connectionTimer->start(1000);
    pingTimer = new QTimer(this);
    connect(pingTimer, &QTimer::timeout, this, &ChatWindow::updatePing);
    pingTimer->start(5000);
    updatePing();

    connect(loginButton, &QPushButton::clicked, this, &ChatWindow::onConnectClicked);
    connect(loginPassEdit, &QLineEdit::returnPressed, this, &ChatWindow::onConnectClicked);

    if (!userValue.isEmpty() && !passValue.isEmpty()) {
        loginUserEdit->setText(userValue);
        loginPassEdit->setText(passValue);
        startConnection();
    }

    // prepare stream audio output sink lazily when we first play stream audio
}

ChatWindow::~ChatWindow() {
    if (encoderThread.isRunning()) {
        encoderThread.quit();
        encoderThread.wait(500);
    }
}

void ChatWindow::appendLog(const QString &text) {
    chatView->append(text);
    scrollChatToBottom();
}

void ChatWindow::onConnectClicked() {
    userValue = loginUserEdit->text().trimmed();
    passValue = loginPassEdit->text();
    if (userValue.isEmpty() || passValue.isEmpty()) {
        updateLoginStatus("Enter login and password", "red");
        return;
    }
    updateLoginStatus("Connecting...", "orange");
    savePersistentConfig();
    startConnection();
}

void ChatWindow::onSocketConnected() {
    const QString user = userValue;
    const QString pass = passValue;
    updateLoginStatus("Authorizing...", "orange");
    auth.login(user, pass);
    loginUserEdit->setEnabled(false);
    loginPassEdit->setEnabled(false);
    loginButton->setEnabled(false);
}

void ChatWindow::onSocketDisconnected() {
    loggedIn = false;
    setLoggedInUi(false);
    updateLoginStatus("Disconnected", "red");
    loginUserEdit->setEnabled(true);
    loginPassEdit->setEnabled(true);
    loginButton->setEnabled(true);
}

void ChatWindow::onSocketError(QAbstractSocket::SocketError) {
    appendLog(QStringLiteral("Connection error: %1").arg(socket.errorString()));
    setLoggedInUi(false);
    updateLoginStatus(QStringLiteral("Error: %1").arg(socket.errorString()), "red");
    loginUserEdit->setEnabled(true);
    loginPassEdit->setEnabled(true);
    loginButton->setEnabled(true);
}

void ChatWindow::onLoginResult(bool ok) {
    if (ok) {
        loggedIn = true;
        chat.setSender(auth.currentUsername());
        userLabel->setText(QStringLiteral("User: %1").arg(auth.currentUsername()));
        setLoggedInUi(true);
        updateLoginStatus("Logged in", "green");
        loginUserEdit->setEnabled(true);
        loginPassEdit->setEnabled(true);
        loginButton->setEnabled(true);
        sendHistoryRequest();
        requestUsersList();
        savePersistentConfig();
        showNormal();
        raise();
        activateWindow();
    } else {
        setLoggedInUi(false);
        updateLoginStatus("Login failed. Try again.", "red");
        loginUserEdit->setEnabled(true);
        loginPassEdit->setEnabled(true);
        loginButton->setEnabled(true);
    }
}

void ChatWindow::onSendClicked() {
    if (!loggedIn) {
        return;
    }
    const QString text = messageEdit->text().trimmed();
    if (text.isEmpty()) return;
    chat.sendMessage(text);
    messageEdit->clear();
}

void ChatWindow::sendHistoryRequest() {
    Message msg;
    msg.type = MessageType::HistoryRequest;
    msg.timestampMs = QDateTime::currentMSecsSinceEpoch();
    socket.write(MessageProtocol::encodeMessage(msg));
}

void ChatWindow::onMessageReceived(const QString &sender, const QString &message, const QString &timestamp) {
    appendLog(QStringLiteral("[%1] %2: %3").arg(timestamp, sender, message));
}

void ChatWindow::onSocketReadyRead() {
    buffer.append(socket.readAll());
    while (buffer.size() >= static_cast<int>(sizeof(quint32))) {
        QDataStream peek(&buffer, QIODevice::ReadOnly);
        peek.setVersion(QDataStream::Qt_DefaultCompiledVersion);
        quint32 frameLength = 0;
        peek >> frameLength;
        const int totalSize = static_cast<int>(frameLength + sizeof(quint32));
        if (buffer.size() < totalSize) break;

        const QByteArray frame = buffer.left(totalSize);
        buffer.remove(0, totalSize);

        Message msg;
        if (!MessageProtocol::decodeMessage(frame, msg)) {
            appendLog("Protocol error: failed to decode message");
            continue;
        }

        if (auth.handleMessage(msg)) {
            continue;
        }
        if (msg.type == MessageType::ChatMessage) {
            chat.receiveMessage(frame);
            continue;
        }
        if (msg.type == MessageType::HistoryResponse) {
            const QStringList lines = QString::fromUtf8(msg.payload).split("\n", Qt::SkipEmptyParts);
            for (const auto &line : lines) {
                chatView->append(line);
            }
            scrollChatToBottom();
            continue;
        }
    if (msg.type == MessageType::UsersListResponse) {
        userList->clear();
        const QStringList users = QString::fromUtf8(msg.payload).split("\n", Qt::SkipEmptyParts);
        for (const QString &u : users) {
            QListWidgetItem *it = new QListWidgetItem(streamingUsers.contains(u) ? QStringLiteral("%1  [LIVE]").arg(u) : u, userList);
            it->setData(Qt::UserRole, u);
        }
        continue;
    }
        if (msg.type == MessageType::VoiceChunk) {
            if (msg.sender != auth.currentUsername()) {
                voice.playReceivedAudio(msg.payload);
            }
            continue;
        }
        if (msg.type == MessageType::StreamAudio) {
            ensureStreamAudioOutput();
            if (streamAudioOutputDevice) {
                // payload: seq (u32 big endian) + ts (qint64 big endian) + PCM data
                if (msg.payload.size() > static_cast<int>(sizeof(quint32) + sizeof(qint64))) {
                    QByteArray pcm = msg.payload.mid(sizeof(quint32) + sizeof(qint64));
                    streamAudioOutputDevice->write(pcm);
                }
            }
            continue;
        }
        if (msg.type == MessageType::ScreenFrame) {
            if (msg.payload.isEmpty()) {
                streamingUsers.remove(msg.sender);
                streamFrames.remove(msg.sender);
                lastFrameIdReceived.remove(msg.sender);
                if (watchingRemote && currentStreamUser == msg.sender) {
                    watchingRemote = false;
                    currentStreamUser.clear();
                    sharePreview->clear();
                    sharePreview->setVisible(false);
                    streamVolumeLabel->setVisible(false);
                    streamVolumeSlider->setVisible(false);
                }
                updateUserListDisplay();
                continue;
            }
            QDataStream ds(msg.payload);
            ds.setByteOrder(QDataStream::BigEndian);
            quint32 frameId = 0;
            ds >> frameId;
            QByteArray encoded = msg.payload.mid(static_cast<int>(sizeof(quint32)));
            if (frameId == 0) {
                // configuration (SPS/PPS) packet
                h264Decoder.setConfig(encoded);
                if (!decoderReady) decoderReady = h264Decoder.init();
                continue;
            }
            quint32 &lastId = lastFrameIdReceived[msg.sender];
            if (frameId <= lastId) {
                continue; // stale
            }
            lastId = frameId;
            if (!decoderReady) decoderReady = h264Decoder.init();
            QImage image = decoderReady ? h264Decoder.decode(encoded) : QImage();
            if (!image.isNull()) {
                QPixmap pix = QPixmap::fromImage(image);
                streamFrames[msg.sender] = pix;
                streamingUsers.insert(msg.sender);
                if (watchingRemote && currentStreamUser == msg.sender) {
                    sharePreview->setFrame(pix);
                    sharePreview->setVisible(true);
                    streamVolumeLabel->setVisible(true);
                    streamVolumeSlider->setVisible(true);
                }
                updateUserListDisplay();
            }
            continue;
        }
        if (msg.type == MessageType::Error) {
            appendLog(QStringLiteral("Server error: %1").arg(QString::fromUtf8(msg.payload)));
        }
    }
}

void ChatWindow::onMicToggle() {
    if (micMuted) {
        muteButton->setChecked(true);
        updateMuteButtonState(true);
        return;
    }
    if (!micOn) {
        if (voice.restartInput()) {
            micOn = true;
            updateMicButtonState(true);
        } else {
            micOn = false;
            micToggleButton->setChecked(false);
            updateMicButtonState(false);
        }
    } else {
        voice.stopVoiceTransmission();
        micOn = false;
        micToggleButton->setChecked(false);
        updateMicButtonState(false);
    }
}

void ChatWindow::onMicVolume(int value) {
    micVolume = value / 100.0;
    voice.setInputVolume(micVolume);
    savePersistentConfig();
}

void ChatWindow::onOutputVolume(int value) {
    outputVolume = value / 100.0;
    voice.setOutputVolume(outputVolume);
    savePersistentConfig();
}

void ChatWindow::onScreenShareStart() {
    if (!screenShare.isCapturing()) {
        shareToggleButton->setChecked(true);
        updateShareButtonState(true);
        screenShare.setFps(shareFps);
        h264Encoder.init(shareResolution.width(), shareResolution.height(), shareFps, calcShareBitrate());
        streamConfigSent = false;
        screenShare.startCapturing(1000 / shareFps);
        startStreamAudioCapture();
    }
}

void ChatWindow::onScreenShareStop() {
    if (screenShare.isCapturing()) {
        screenShare.stopCapturing();
    }
    stopStreamAudioCapture();
    shareToggleButton->setChecked(false);
    updateShareButtonState(false);
    if (!watchingRemote) {
        sharePreview->clear();
        sharePreview->setPlaceholder("Screen preview");
        sharePreview->setVisible(false);
        isLocalSharingPreviewVisible = false;
    }
    // notify others that stream ended via empty frame
    Message msg;
    msg.type = MessageType::ScreenFrame;
    msg.timestampMs = QDateTime::currentMSecsSinceEpoch();
    msg.payload.clear();
    socket.write(MessageProtocol::encodeMessage(msg));
    streamingUsers.remove(auth.currentUsername());
    streamFrames.remove(auth.currentUsername());
    streamConfigSent = false;
    if (watchingRemote && currentStreamUser == auth.currentUsername()) {
        watchingRemote = false;
        currentStreamUser.clear();
        sharePreview->clear();
        sharePreview->setVisible(false);
    }
    updateUserListDisplay();
}

void ChatWindow::onFrameReady(const QPixmap &frame) {
    if (frame.isNull()) return;
    QPixmap pix = frame; // keep original size for encoding
    // local preview for own stream (scaled in paint)
    sharePreview->setFrame(pix);
    sharePreview->setVisible(true);
    isLocalSharingPreviewVisible = true;
    if (!loggedIn) return;
    // enqueue for encoding in worker thread
    quint32 frameId = ++lastFrameIdSent;
    qint64 ts = QDateTime::currentMSecsSinceEpoch();
    if (encodingInProgress) {
        pendingFrame = frame.toImage();
        pendingFrameId = frameId;
        pendingFrameTimestamp = ts;
        hasPendingFrame = true;
        return;
    }
    encodingInProgress = true;
    QMetaObject::invokeMethod(encoderWorker, "encodeFrame", Qt::QueuedConnection,
                              Q_ARG(QImage, frame.toImage()),
                              Q_ARG(QSize, shareResolution),
                              Q_ARG(int, shareQuality),
                              Q_ARG(quint32, frameId),
                              Q_ARG(qint64, ts));
}

void ChatWindow::onEncodedFrame(const QByteArray &data, quint32 frameId, qint64 timestampMs) {
    const qint64 backlogLimit = 256 * 1024; // tighten backlog to reduce lag
    if (socket.bytesToWrite() < backlogLimit && loggedIn) {
        if (!streamConfigSent) {
            const QByteArray cfg = h264Encoder.config();
            if (!cfg.isEmpty()) {
                QByteArray cfgPayload;
                QDataStream dsCfg(&cfgPayload, QIODevice::WriteOnly);
                dsCfg.setByteOrder(QDataStream::BigEndian);
                dsCfg << static_cast<quint32>(0);
                cfgPayload.append(cfg);
                Message cfgMsg;
                cfgMsg.type = MessageType::ScreenFrame;
                cfgMsg.timestampMs = timestampMs;
                cfgMsg.payload = cfgPayload;
                socket.write(MessageProtocol::encodeMessage(cfgMsg));
                streamConfigSent = true;
            }
        }
        QByteArray payload;
        QDataStream ds(&payload, QIODevice::WriteOnly);
        ds.setByteOrder(QDataStream::BigEndian);
        ds << frameId;
        payload.append(data);

        Message msg;
        msg.type = MessageType::ScreenFrame;
        msg.timestampMs = timestampMs;
        msg.payload = payload;
        socket.write(MessageProtocol::encodeMessage(msg));
    }
    encodingInProgress = false;
    if (hasPendingFrame) {
        hasPendingFrame = false;
        encodingInProgress = true;
        QMetaObject::invokeMethod(encoderWorker, "encodeFrame", Qt::QueuedConnection,
                                  Q_ARG(QImage, pendingFrame),
                                  Q_ARG(QSize, shareResolution),
                                  Q_ARG(int, shareQuality),
                                  Q_ARG(quint32, pendingFrameId),
                                  Q_ARG(qint64, pendingFrameTimestamp));
    }
}

void ChatWindow::onFullscreenToggle() {
    const QPixmap pix = sharePreview->currentFrame();
    if (pix.isNull()) {
        return;
    }
    QWidget *viewer = new QWidget();
    viewer->setAttribute(Qt::WA_DeleteOnClose);
    viewer->setWindowTitle("Screen preview");
    auto layout = new QVBoxLayout(viewer);
    QLabel *label = new QLabel(viewer);
    label->setAlignment(Qt::AlignCenter);
    label->setPixmap(pix.scaled(QGuiApplication::primaryScreen()->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    layout->addWidget(label);
    viewer->showFullScreen();
}

void ChatWindow::onShareConfig() {
    if (screenShare.isCapturing()) {
        onScreenShareStop();
        return;
    }
    int fps = shareFps;
    QSize res = shareResolution;
    int quality = shareQuality;
    if (openShareDialog(fps, res, quality)) {
        shareFps = fps;
        shareResolution = res;
        shareQuality = quality;
        screenShare.setTargetSize(shareResolution);
        screenShare.setFps(shareFps);
        h264Encoder.init(shareResolution.width(), shareResolution.height(), shareFps, calcShareBitrate());
        streamConfigSent = false;
        savePersistentConfig();
        onScreenShareStart();
    }
}

void ChatWindow::onUserContextMenuRequested(const QPoint &pos) {
    QListWidgetItem *item = userList->itemAt(pos);
    if (!item) return;
    const QString uname = item->data(Qt::UserRole).toString();
    QMenu menu(this);
    if (streamingUsers.contains(uname)) {
        QAction *watchAct = menu.addAction("Watch stream");
        connect(watchAct, &QAction::triggered, this, [this, uname]() {
            currentStreamUser = uname;
            watchingRemote = true;
            if (streamFrames.contains(uname)) {
                sharePreview->setFrame(streamFrames[uname]);
            } else {
                sharePreview->setPlaceholder("Waiting for frames...");
            }
            sharePreview->setVisible(true);
            streamVolumeLabel->setVisible(true);
            streamVolumeSlider->setVisible(true);
        });
    }
    menu.exec(userList->viewport()->mapToGlobal(pos));
}

void ChatWindow::onLogout() {
    if (socket.state() == QAbstractSocket::ConnectedState) {
        setLoggedInUi(false);
        socket.disconnectFromHost();
        streamFrames.clear();
        streamingUsers.clear();
        stopStreamAudioCapture();
        updateUserListDisplay();
        updateLoginStatus("Logged out", "orange");
    } else {
        startConnection();
    }
}

void ChatWindow::onOpenSettings() {
    QDialog dlg(this);
    dlg.setWindowTitle("Settings");
    dlg.setMinimumSize(420, 320);
    dlg.setSizeGripEnabled(true);
    QVBoxLayout root(&dlg);
    QTabWidget tabs(&dlg);
    QWidget serverTab(&dlg);
    QWidget audioTab(&dlg);

    // Server tab
    QFormLayout serverForm(&serverTab);
    QLineEdit *hostEdit = new QLineEdit(hostValue, &serverTab);
    QLineEdit *portEdit = new QLineEdit(QString::number(portValue), &serverTab);
    serverForm.addRow("Server IP", hostEdit);
    serverForm.addRow("Server Port", portEdit);
    serverTab.setLayout(&serverForm);

    // Audio tab
    QFormLayout audioForm(&audioTab);
    QComboBox *inputDevices = new QComboBox(&audioTab);
    QComboBox *outputDevices = new QComboBox(&audioTab);
    for (const QAudioDevice &dev : QMediaDevices::audioInputs()) {
        inputDevices->addItem(dev.description(), QVariant::fromValue(dev));
    }
    for (const QAudioDevice &dev : QMediaDevices::audioOutputs()) {
        outputDevices->addItem(dev.description(), QVariant::fromValue(dev));
    }
    auto selectById = [](QComboBox *box, const QByteArray &id) {
        if (id.isEmpty()) return;
        for (int i = 0; i < box->count(); ++i) {
            QAudioDevice dev = box->itemData(i).value<QAudioDevice>();
            if (dev.id() == id) {
                box->setCurrentIndex(i);
                break;
            }
        }
    };
    inputDevices->setCurrentIndex(0);
    outputDevices->setCurrentIndex(0);
    selectById(inputDevices, inputDeviceId);
    selectById(outputDevices, outputDeviceId);

    QSlider *micVol = new QSlider(Qt::Horizontal, &audioTab);
    micVol->setRange(0, 100);
    micVol->setValue(static_cast<int>(micVolume * 100));
    QSlider *outVol = new QSlider(Qt::Horizontal, &audioTab);
    outVol->setRange(0, 100);
    outVol->setValue(static_cast<int>(outputVolume * 100));

    audioForm.addRow("Input device", inputDevices);
    audioForm.addRow("Output device", outputDevices);
    audioForm.addRow("Mic volume", micVol);
    audioForm.addRow("Output volume", outVol);
    audioTab.setLayout(&audioForm);

    tabs.addTab(&serverTab, "Server");
    tabs.addTab(&audioTab, "Audio");

    root.addWidget(&tabs);
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    root.addWidget(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        const QString newHost = hostEdit->text().trimmed();
        if (!newHost.isEmpty()) hostValue = newHost;
        portValue = static_cast<quint16>(portEdit->text().toUShort());

        micVolume = micVol->value() / 100.0;
        outputVolume = outVol->value() / 100.0;
        voice.setInputVolume(micVolume);
        voice.setOutputVolume(outputVolume);
        voice.setVolumes(micVolume, outputVolume);

        QAudioDevice inDev = inputDevices->currentData().value<QAudioDevice>();
        QAudioDevice outDev = outputDevices->currentData().value<QAudioDevice>();
        inputDeviceId = inDev.id();
        outputDeviceId = outDev.id();
        voice.setInputDevice(inDev);
        voice.setOutputDevice(outDev);
        if (micOn && !micMuted) {
            voice.restartInput();
        }
        savePersistentConfig();
    }
}


bool ChatWindow::openShareDialog(int &fpsOut, QSize &resolutionOut, int &qualityOut) {
    QDialog dlg(this);
    dlg.setWindowTitle("Share Screen Settings");
    dlg.setMinimumSize(380, 260);
    dlg.setSizeGripEnabled(true);
    QFormLayout form(&dlg);

    QComboBox *fpsBox = new QComboBox(&dlg);
    fpsBox->addItems({"5", "10", "15", "30"});
    int fpsIndex = fpsBox->findText(QString::number(shareFps));
    if (fpsIndex >= 0) fpsBox->setCurrentIndex(fpsIndex);

    QComboBox *resBox = new QComboBox(&dlg);
    resBox->addItem("640 x 360", QSize(640, 360));
    resBox->addItem("1280 x 720 (HD)", QSize(1280, 720));
    resBox->addItem("1920 x 1080 (FullHD)", QSize(1920, 1080));
    int bestResIndex = 0;
    for (int i = 0; i < resBox->count(); ++i) {
        if (resBox->itemData(i).toSize() == shareResolution) {
            bestResIndex = i;
            break;
        }
    }
    resBox->setCurrentIndex(bestResIndex);

    QComboBox *qualityBox = new QComboBox(&dlg);
    qualityBox->addItems({"50", "60", "75", "85"});
    int qIndex = qualityBox->findText(QString::number(shareQuality));
    if (qIndex >= 0) qualityBox->setCurrentIndex(qIndex);

    form.addRow("FPS", fpsBox);
    form.addRow("Resolution", resBox);
    form.addRow("JPEG quality", qualityBox);
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return false;

    fpsOut = fpsBox->currentText().toInt();
    resolutionOut = resBox->currentData().toSize();
    qualityOut = qualityBox->currentText().toInt();
    return true;
}

void ChatWindow::startConnection() {
    loggedIn = false;
    setLoggedInUi(false);
    chatView->clear();
    watchingRemote = false;
    currentStreamUser.clear();
    sharePreview->setPlaceholder("Screen preview");
    streamVolumeLabel->setVisible(false);
    streamVolumeSlider->setVisible(false);
    mainPanel->setVisible(false);
    updateLoginStatus("Connecting...", "orange");
    lastFrameIdSent = 0;
    lastFrameIdReceived.clear();
    hasPendingFrame = false;
    encodingInProgress = false;
    streamConfigSent = false;
    decoderReady = h264Decoder.init();

    buffer.clear();
    socket.disconnectFromHost();
    // qDebug() << "Connecting to" << hostValue << ":" << portValue;
    socket.connectToHost(hostValue, portValue);
}

void ChatWindow::setLoggedInUi(bool enabled) {
    sendButton->setEnabled(enabled);
    micToggleButton->setEnabled(enabled);
    shareToggleButton->setEnabled(enabled);
    messageEdit->setEnabled(enabled);
    settingsButton->setEnabled(enabled);
    logoutButton->setEnabled(true); // allow reconnect
    mainPanel->setVisible(enabled);
    if (loginPanel) loginPanel->setVisible(!enabled);
    loginStatusLabel->setVisible(!enabled && !loginStatusLabel->text().isEmpty());
}

void ChatWindow::requestUsersList() {
    Message msg;
    msg.type = MessageType::UsersListRequest;
    msg.timestampMs = QDateTime::currentMSecsSinceEpoch();
    socket.write(MessageProtocol::encodeMessage(msg));
}

void ChatWindow::updateUserListDisplay() {
    for (int i = 0; i < userList->count(); ++i) {
        QListWidgetItem *it = userList->item(i);
        const QString uname = it->data(Qt::UserRole).toString();
        it->setText(streamingUsers.contains(uname) ? QStringLiteral("%1  [LIVE]").arg(uname) : uname);
    }
}

void ChatWindow::updateConnectionStatus() {
    QString text;
    QString color = "gray";
    switch (socket.state()) {
    case QAbstractSocket::ConnectedState:
        text = "Connected";
        color = "green";
        logoutButton->setText("Logout");
        break;
    case QAbstractSocket::ConnectingState:
        text = "Connecting...";
        color = "orange";
        logoutButton->setText("Cancel");
        break;
    default:
        text = "Disconnected";
        color = "red";
        logoutButton->setText("Reconnect");
        break;
    }
    connectionLabel->setText(text);
    connectionLabel->setStyleSheet(QStringLiteral("color:%1").arg(color));
    if (loginConnectionLabel) {
        loginConnectionLabel->setText(text);
        loginConnectionLabel->setStyleSheet(QStringLiteral("color:%1").arg(color));
    }
    const bool connected = socket.state() == QAbstractSocket::ConnectedState;
    pingLabel->setVisible(connected);
    if (!connected) {
        pingLabel->setText("Ping: n/a");
        pingLabel->setStyleSheet("color: gray");
    }
}

void ChatWindow::updatePing() {
    if (socket.state() != QAbstractSocket::ConnectedState) {
        pingLabel->setVisible(false);
        return;
    }
    pingLabel->setVisible(true);
    auto process = new QProcess(this);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, process](int, QProcess::ExitStatus) {
        const QString output = QString::fromLocal8Bit(process->readAllStandardOutput());
        process->deleteLater();
        QRegularExpression re("time[=<]\\s*(\\d+)ms", QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch match = re.match(output);
        if (socket.state() != QAbstractSocket::ConnectedState) {
            pingLabel->setText("Ping: n/a");
            pingLabel->setStyleSheet("color: gray");
            return;
        }
        if (match.hasMatch()) {
            pingLabel->setText(QStringLiteral("Ping: %1 ms").arg(match.captured(1)));
            pingLabel->setStyleSheet("color: green");
        } else {
            pingLabel->setText("Ping: n/a");
            pingLabel->setStyleSheet("color: red");
        }
    });
    process->start("ping", {"-n", "1", hostValue});
}

void ChatWindow::updateMicButtonState(bool on) {
    micToggleButton->setChecked(on);
    micToggleButton->setIcon(QIcon(on ? ":/icons/icons/mic-on.svg" : ":/icons/icons/mic-off.svg"));
    micToggleButton->setStyleSheet(on ? "background-color:#4caf50;color:white;" : "background-color:#444;color:#ccc;");
}

void ChatWindow::updateShareButtonState(bool on) {
    shareToggleButton->setChecked(on);
    shareToggleButton->setStyleSheet(on ? "background-color:#4caf50;color:white;" : "background-color:#444;color:#ccc;");
    shareToggleButton->setIcon(QIcon(":/icons/icons/screen-sharing.svg"));
}

void ChatWindow::onMuteToggle() {
    micMuted = muteButton->isChecked();
    updateMuteButtonState(micMuted);
    voice.setPlaybackEnabled(!micMuted);
    if (micMuted && micOn) {
        voice.stopVoiceTransmission();
        micOn = false;
        updateMicButtonState(false);
    }
}

void ChatWindow::updateMuteButtonState(bool on) {
    muteButton->setChecked(on);
    muteButton->setIcon(QIcon(on ? ":/icons/icons/sound-off.svg" : ":/icons/icons/sound.svg"));
    muteButton->setStyleSheet(on ? "background-color:#d32f2f;color:white;" : "background-color:#444;color:#ccc;");
}

void ChatWindow::updateLoginStatus(const QString &text, const QString &color) {
    loginStatusLabel->setText(text);
    loginStatusLabel->setStyleSheet(QStringLiteral("color:%1").arg(color));
    loginStatusLabel->setVisible(!text.isEmpty());
}

void ChatWindow::loadPersistentConfig() {
    const QString settingsPath = QDir(QCoreApplication::applicationDirPath()).filePath("settings.ini");
    QSettings settings(settingsPath, QSettings::IniFormat);
    const QString defaultHost = "213.171.26.107";
    hostValue = settings.value("network/host", defaultHost).toString();
    portValue = static_cast<quint16>(settings.value("network/port", portValue).toUInt());
    userValue = settings.value("auth/user", userValue).toString();
    passValue = settings.value("auth/pass", passValue).toString();

    shareFps = settings.value("share/fps", shareFps).toInt();
    const int resW = settings.value("share/resW", shareResolution.width()).toInt();
    const int resH = settings.value("share/resH", shareResolution.height()).toInt();
    shareResolution = QSize(resW, resH);
    shareQuality = settings.value("share/quality", shareQuality).toInt();

    micVolume = settings.value("audio/micVolume", micVolume).toDouble();
    outputVolume = settings.value("audio/outputVolume", outputVolume).toDouble();
    micVolume = std::clamp(micVolume, 0.0, 1.0);
    outputVolume = std::clamp(outputVolume, 0.0, 1.0);
    inputDeviceId = settings.value("audio/inputId").toByteArray();
    outputDeviceId = settings.value("audio/outputId").toByteArray();
}

void ChatWindow::savePersistentConfig() {
    const QString settingsPath = QDir(QCoreApplication::applicationDirPath()).filePath("settings.ini");
    QSettings settings(settingsPath, QSettings::IniFormat);
    settings.setValue("network/host", hostValue);
    settings.setValue("network/port", portValue);
    settings.setValue("auth/user", userValue);
    settings.setValue("auth/pass", passValue);

    settings.setValue("share/fps", shareFps);
    settings.setValue("share/resW", shareResolution.width());
    settings.setValue("share/resH", shareResolution.height());
    settings.setValue("share/quality", shareQuality);

    settings.setValue("audio/micVolume", micVolume);
    settings.setValue("audio/outputVolume", outputVolume);
    settings.setValue("audio/inputId", inputDeviceId);
    settings.setValue("audio/outputId", outputDeviceId);
}

int ChatWindow::calcShareBitrate() const {
    // Simple heuristic: base bitrate per megapixel scaled by fps and quality
    const double mpix = (shareResolution.width() * shareResolution.height()) / 1000000.0;
    double fpsScale = std::max(1.0, shareFps / 30.0);
    double qualityScale = 1.0;
    if (shareQuality >= 85) qualityScale = 3.0;
    else if (shareQuality >= 75) qualityScale = 2.2;
    else if (shareQuality >= 60) qualityScale = 1.6;
    else qualityScale = 1.2;
    const double baseMbps = 1.2; // for ~1MP @30fps medium quality
    int bitrate = static_cast<int>(baseMbps * mpix * fpsScale * qualityScale * 1000000.0);
    return std::clamp(bitrate, 400000, 8000000);
}

void ChatWindow::scrollChatToBottom() {
    QTextCursor cursor = chatView->textCursor();
    cursor.movePosition(QTextCursor::End);
    chatView->setTextCursor(cursor);
    chatView->ensureCursorVisible();
}

void ChatWindow::startStreamAudioCapture() {
    stopStreamAudioCapture();
    QAudioFormat fmt;
    fmt.setSampleRate(48000);
    fmt.setChannelCount(2);
    fmt.setSampleFormat(QAudioFormat::Int16);
    QAudioDevice loopDev = QMediaDevices::defaultAudioInput();
    streamAudioInput = new QAudioSource(loopDev, fmt, this);
    streamAudioInput->setBufferSize(4096);
    streamAudioInputDevice = streamAudioInput->start();
    if (streamAudioInputDevice) {
        connect(streamAudioInputDevice, &QIODevice::readyRead, this, [this]() {
            if (!loggedIn || !screenShare.isCapturing()) return;
            QByteArray pcm = streamAudioInputDevice->readAll();
            if (pcm.isEmpty()) return;
            QByteArray payload;
            QDataStream ds(&payload, QIODevice::WriteOnly);
            ds.setByteOrder(QDataStream::BigEndian);
            ds << ++streamAudioSeq;
            ds << QDateTime::currentMSecsSinceEpoch();
            payload.append(pcm);
            Message msg;
            msg.type = MessageType::StreamAudio;
            msg.timestampMs = QDateTime::currentMSecsSinceEpoch();
            msg.payload = payload;
            socket.write(MessageProtocol::encodeMessage(msg));
        });
    }
}

void ChatWindow::stopStreamAudioCapture() {
    if (streamAudioInput) {
        streamAudioInput->stop();
        streamAudioInput->deleteLater();
        streamAudioInput = nullptr;
        streamAudioInputDevice = nullptr;
    }
    streamAudioSeq = 0;
}

void ChatWindow::ensureStreamAudioOutput() {
    if (streamAudioOutput && streamAudioOutputDevice) return;
    QAudioFormat fmt;
    fmt.setSampleRate(48000);
    fmt.setChannelCount(2);
    fmt.setSampleFormat(QAudioFormat::Int16);
    QAudioDevice outDev = QMediaDevices::defaultAudioOutput();
    streamAudioOutput = new QAudioSink(outDev, fmt, this);
    streamAudioOutput->setBufferSize(4096 * 4);
    streamAudioOutputDevice = streamAudioOutput->start();
}

void ChatWindow::handleAudioDevicesChanged(bool inputsChanged) {
    if (inputsChanged) {
        // If selected input disappeared, switch to default and restart if needed
        const QList<QAudioDevice> inputs = QMediaDevices::audioInputs();
        auto findById = [&](const QByteArray &id) -> QAudioDevice {
            for (const auto &d : inputs) if (d.id() == id) return d;
            return QAudioDevice();
        };
        if (!inputDeviceId.isEmpty() && findById(inputDeviceId).isNull()) {
            QAudioDevice def = QMediaDevices::defaultAudioInput();
            inputDeviceId = def.id();
            voice.setInputDevice(def);
            if (micOn && !micMuted) voice.restartInput();
        }
    } else {
        const QList<QAudioDevice> outputs = QMediaDevices::audioOutputs();
        auto findById = [&](const QByteArray &id) -> QAudioDevice {
            for (const auto &d : outputs) if (d.id() == id) return d;
            return QAudioDevice();
        };
        if (!outputDeviceId.isEmpty() && findById(outputDeviceId).isNull()) {
            QAudioDevice def = QMediaDevices::defaultAudioOutput();
            outputDeviceId = def.id();
            voice.setOutputDevice(def);
            voice.restartOutput();
        }
    }
}
