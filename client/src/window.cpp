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

#include "message_protocol.h"
#include "types.h"

ChatWindow::ChatWindow(QWidget *parent) : QWidget(parent) {
    hostValue = qEnvironmentVariable("APP_HOST", "vagabovnr.ru");
    portValue = static_cast<quint16>(qEnvironmentVariableIntValue("APP_PORT", nullptr) ? qEnvironmentVariableIntValue("APP_PORT", nullptr) : 12345);
    userValue = qEnvironmentVariable("APP_USER", "demo");
    passValue = qEnvironmentVariable("APP_PASS", "demo");
    // Registration handled externally (e.g., via Telegram bot); client only logs in.

    sendButton = new QPushButton("Send", this);
    micToggleButton = new QPushButton("Mic Off", this);
    micToggleButton->setCheckable(true);
    shareToggleButton = new QPushButton("Share Screen", this);
    shareToggleButton->setCheckable(true);
    logoutButton = new QPushButton("Logout", this);
    settingsButton = new QPushButton("Settings", this);
    watchButton = new QPushButton("Watch", this);

    sendButton->setEnabled(false);
    micToggleButton->setEnabled(false);
    shareToggleButton->setEnabled(false);

    chatView = new QTextEdit(this);
    chatView->setReadOnly(true);
    messageEdit = new QLineEdit(this);
    sharePreview = new QLabel(this);
    sharePreview->setMinimumHeight(160);
    sharePreview->setAlignment(Qt::AlignCenter);
    sharePreview->setStyleSheet("background:#222; color:#ccc;");
    sharePreview->setText("Screen preview");
    userList = new QListWidget(this);
    userList->setMinimumWidth(160);
    userList->setContextMenuPolicy(Qt::CustomContextMenu);
    streamSelector = new QComboBox(this);
    streamSelector->addItem("Local preview");

    userLabel = new QLabel("User: -", this);
    connectionLabel = new QLabel("Disconnected", this);

    auto topLayout = new QHBoxLayout();
    topLayout->addWidget(userLabel);
    topLayout->addWidget(connectionLabel);
    topLayout->addStretch();
    topLayout->addWidget(settingsButton);
    topLayout->addWidget(logoutButton);

    auto controlsLayout = new QHBoxLayout();
    controlsLayout->addWidget(shareToggleButton);
    controlsLayout->addWidget(micToggleButton);
    controlsLayout->addWidget(watchButton);
    controlsLayout->addWidget(new QLabel("Stream:", this));
    controlsLayout->addWidget(streamSelector);

    auto bottomLayout = new QHBoxLayout();
    bottomLayout->addWidget(messageEdit);
    bottomLayout->addWidget(sendButton);

    auto mainLayout = new QHBoxLayout();
    mainLayout->addWidget(userList, /*stretch*/1);
    userList->setMaximumWidth(50);
    mainLayout->addWidget(sharePreview, /*stretch*/3);


    auto outer = new QVBoxLayout();
    outer->addLayout(topLayout);
    outer->addLayout(controlsLayout);
    outer->addLayout(mainLayout);
    outer->addWidget(new QLabel("Chat", this));
    outer->addWidget(chatView);
    chatView->setMaximumHeight(100);
    outer->addLayout(bottomLayout);
    setLayout(outer);

    auth.setSocket(&socket);

    connect(sendButton, &QPushButton::clicked, this, &ChatWindow::onSendClicked);
    connect(micToggleButton, &QPushButton::clicked, this, &ChatWindow::onMicToggle);
    connect(shareToggleButton, &QPushButton::clicked, this, &ChatWindow::onShareConfig);
    connect(logoutButton, &QPushButton::clicked, this, &ChatWindow::onLogout);
    connect(settingsButton, &QPushButton::clicked, this, &ChatWindow::onOpenSettings);
    connect(watchButton, &QPushButton::clicked, this, [this]() {
        if (watchingRemote) {
            streamSelector->setCurrentIndex(0);
            watchingRemote = false;
            watchButton->setText("Watch");
        } else {
            if (streamSelector->currentIndex() == 0 && streamSelector->count() > 1) {
                streamSelector->setCurrentIndex(1);
            }
            if (streamSelector->currentIndex() > 0) {
                watchingRemote = true;
                watchButton->setText("Stop watching");
            }
        }
    });
    connect(&socket, &QTcpSocket::readyRead, this, &ChatWindow::onSocketReadyRead);
    connect(&socket, &QTcpSocket::connected, this, &ChatWindow::onSocketConnected);
    connect(&socket, &QTcpSocket::disconnected, this, &ChatWindow::onSocketDisconnected);
    connect(&socket, &QTcpSocket::errorOccurred, this, &ChatWindow::onSocketError);
    connect(&auth, &Authentication::loginResult, this, &ChatWindow::onLoginResult);
    connect(&auth, &Authentication::loginError, this, [this](const QString &msg) {
        if (showLoginDialog(msg.isEmpty() ? "Login failed" : msg)) {
            startConnection();
        }
    });
    connect(&chat, &Chat::messageToSend, &socket, [this](const QByteArray &data) { socket.write(data); });
    connect(&chat, &Chat::messageReceived, this, &ChatWindow::onMessageReceived);
    connect(&screenShare, &ScreenShare::frameReady, this, &ChatWindow::onFrameReady);
    connect(&screenShare, &ScreenShare::started, []() {});
    connect(&screenShare, &ScreenShare::stopped, []() {});
    connect(streamSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ChatWindow::onStreamSelected);
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
    voice.setPlaybackEnabled(true);
    voice.setVolumes(1.0, 1.0);
    connect(userList, &QListWidget::customContextMenuRequested, this, &ChatWindow::onUserContextMenuRequested);
    connect(userList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        if (!item) return;
        const QString text = item->data(Qt::UserRole).toString();
        if (streamSelector->findText(text) != -1) {
            streamSelector->setCurrentText(text);
            watchingRemote = true;
            watchButton->setText("Stop watching");
        }
    });

    setWindowTitle("Qt Chat Client");
    resize(1100, 700);
    hide();

    connectionTimer = new QTimer(this);
    connect(connectionTimer, &QTimer::timeout, this, &ChatWindow::updateConnectionStatus);
    connectionTimer->start(1000);

    if (showLoginDialog()) {
        startConnection();
    } else {
        QTimer::singleShot(0, this, []() { qApp->quit(); });
    }
}

void ChatWindow::appendLog(const QString &text) {
    chatView->append(text);
}

void ChatWindow::onConnectClicked() {
    if (showLoginDialog()) {
        startConnection();
    }
}

void ChatWindow::onSocketConnected() {
    const QString user = userValue;
    const QString pass = passValue;
    auth.login(user, pass);
}

void ChatWindow::onSocketDisconnected() {
    loggedIn = false;
    setLoggedInUi(false);
    // Show login dialog if we haven't authenticated yet
    if (!loggedIn) {
        if (showLoginDialog("Connection lost")) {
            startConnection();
        } else {
            qApp->quit();
        }
    }
}

void ChatWindow::onSocketError(QAbstractSocket::SocketError) {
    appendLog(QStringLiteral("Connection error: %1").arg(socket.errorString()));
}

void ChatWindow::onLoginResult(bool ok) {
    if (ok) {
        loggedIn = true;
        chat.setSender(auth.currentUsername());
        userLabel->setText(QStringLiteral("User: %1").arg(auth.currentUsername()));
        setLoggedInUi(true);
        sendHistoryRequest();
        requestUsersList();
        showNormal();
        raise();
        activateWindow();
    } else {
        setLoggedInUi(false);
        showLoginDialog("Login failed. Try again.");
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
        if (msg.type == MessageType::ScreenFrame) {
            if (msg.payload.isEmpty()) {
                // Stream stopped
                streamingUsers.remove(msg.sender);
                streamFrames.remove(msg.sender);
                int idx = streamSelector->findText(msg.sender);
                if (idx > 0) {
                    streamSelector->removeItem(idx);
                }
                if (streamSelector->currentText() == msg.sender) {
                    streamSelector->setCurrentIndex(0);
                    sharePreview->setText("Screen preview");
                    watchingRemote = false;
                    watchButton->setText("Watch");
                }
                updateUserListDisplay();
            } else {
                QBuffer imgBuf(const_cast<QByteArray *>(&msg.payload));
                imgBuf.open(QIODevice::ReadOnly);
                QImageReader reader(&imgBuf, "JPG");
                const QImage image = reader.read();
                if (!image.isNull()) {
                    QPixmap pix = QPixmap::fromImage(image);
                    streamFrames[msg.sender] = pix;
                    streamingUsers.insert(msg.sender);
                    if (streamSelector->findText(msg.sender) == -1) {
                        streamSelector->addItem(msg.sender);
                    }
                    if (streamSelector->currentText() == msg.sender) {
                        sharePreview->setPixmap(pix.scaled(sharePreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    }
                    updateUserListDisplay();
                }
            }
            continue;
        }
        if (msg.type == MessageType::Error) {
            appendLog(QStringLiteral("Server error: %1").arg(QString::fromUtf8(msg.payload)));
        }
    }
}

void ChatWindow::onMicToggle() {
    if (!micOn) {
        if (voice.startVoiceTransmission()) {
            micOn = true;
            micToggleButton->setChecked(true);
            micToggleButton->setText("Mic On");
        } else {
            micToggleButton->setChecked(false);
            micToggleButton->setText("Mic Off");
        }
    } else {
        voice.stopVoiceTransmission();
        micOn = false;
        micToggleButton->setChecked(false);
        micToggleButton->setText("Mic Off");
    }
}

void ChatWindow::onMicVolume(int value) {
    voice.setInputVolume(value / 100.0);
}

void ChatWindow::onOutputVolume(int value) {
    voice.setOutputVolume(value / 100.0);
}

void ChatWindow::onScreenShareStart() {
    if (!screenShare.isCapturing()) {
        shareToggleButton->setText("Stop Share");
        shareToggleButton->setChecked(true);
        screenShare.setFps(shareFps);
        screenShare.startCapturing(1000 / shareFps);
    }
}

void ChatWindow::onScreenShareStop() {
    if (screenShare.isCapturing()) {
        screenShare.stopCapturing();
    }
    shareToggleButton->setText("Share Screen");
    shareToggleButton->setChecked(false);
    sharePreview->setText("Screen preview");
    // notify others that stream ended via empty frame
    Message msg;
    msg.type = MessageType::ScreenFrame;
    msg.timestampMs = QDateTime::currentMSecsSinceEpoch();
    msg.payload.clear();
    socket.write(MessageProtocol::encodeMessage(msg));
    streamingUsers.remove(auth.currentUsername());
    streamFrames.remove(auth.currentUsername());
    int idx = streamSelector->findText(auth.currentUsername());
    if (idx > 0) streamSelector->removeItem(idx);
    updateUserListDisplay();
}

void ChatWindow::onFrameReady(const QPixmap &frame) {
    if (!frame.isNull()) {
        sharePreview->setPixmap(frame.scaled(sharePreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        if (!loggedIn) return;
        // Send compressed frame to server
        QImage img = frame.toImage().scaled(shareResolution, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        img.save(&buffer, "JPG", shareQuality);
        Message msg;
        msg.type = MessageType::ScreenFrame;
        msg.timestampMs = QDateTime::currentMSecsSinceEpoch();
        msg.payload = bytes;
        socket.write(MessageProtocol::encodeMessage(msg));
    }
}

void ChatWindow::onFullscreenToggle() {
    const QPixmap pix = sharePreview->pixmap(Qt::ReturnByValue);
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
        onScreenShareStart();
    }
}

void ChatWindow::onUserContextMenuRequested(const QPoint &pos) {
    QListWidgetItem *item = userList->itemAt(pos);
    if (!item) return;
    const QString uname = item->data(Qt::UserRole).toString();
    QMenu menu(this);
    if (streamSelector->findText(uname) != -1 || streamingUsers.contains(uname)) {
        QAction *watchAct = menu.addAction("Watch stream");
        connect(watchAct, &QAction::triggered, this, [this, uname]() {
            int idx = streamSelector->findText(uname);
            if (idx != -1) {
                streamSelector->setCurrentIndex(idx);
                watchingRemote = true;
                watchButton->setText("Stop watching");
            }
        });
    }
    menu.exec(userList->viewport()->mapToGlobal(pos));
}

void ChatWindow::onStreamSelected(int) {
    const QString sel = streamSelector->currentText();
    if (streamFrames.contains(sel)) {
        sharePreview->setPixmap(streamFrames[sel].scaled(sharePreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        watchingRemote = (streamSelector->currentIndex() > 0);
        watchButton->setText(watchingRemote ? "Stop watching" : "Watch");
    } else {
        sharePreview->setText("Screen preview");
        watchingRemote = false;
        watchButton->setText("Watch");
    }
}

void ChatWindow::onLogout() {
    if (socket.state() == QAbstractSocket::ConnectedState) {
        setLoggedInUi(false);
        socket.disconnectFromHost();
        streamFrames.clear();
        streamingUsers.clear();
        streamSelector->clear();
        streamSelector->addItem("Local preview");
        updateUserListDisplay();
    } else {
        startConnection();
    }
}

void ChatWindow::onOpenSettings() {
    QDialog dlg(this);
    dlg.setWindowTitle("Audio Settings");
    QFormLayout form(&dlg);

    QComboBox *inputDevices = new QComboBox(&dlg);
    QComboBox *outputDevices = new QComboBox(&dlg);
    for (const QAudioDevice &dev : QMediaDevices::audioInputs()) {
        inputDevices->addItem(dev.description());
    }
    for (const QAudioDevice &dev : QMediaDevices::audioOutputs()) {
        outputDevices->addItem(dev.description());
    }
    inputDevices->setCurrentIndex(0);
    outputDevices->setCurrentIndex(0);

    QSlider *micVol = new QSlider(Qt::Horizontal, &dlg);
    micVol->setRange(0, 100);
    micVol->setValue(100);
    QSlider *outVol = new QSlider(Qt::Horizontal, &dlg);
    outVol->setRange(0, 100);
    outVol->setValue(100);

    form.addRow("Input device", inputDevices);
    form.addRow("Output device", outputDevices);
    form.addRow("Mic volume", micVol);
    form.addRow("Output volume", outVol);

    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        voice.setInputVolume(micVol->value() / 100.0);
        voice.setOutputVolume(outVol->value() / 100.0);
        voice.setVolumes(micVol->value() / 100.0, outVol->value() / 100.0);
        // Note: switching devices would require recreating QAudioSource/QAudioSink with selected dev.
    }
}


bool ChatWindow::showLoginDialog(const QString &error) {
    QDialog dlg(this);
    dlg.setWindowTitle("Login / Register");
    dlg.setWindowFlag(Qt::Window, true);
    dlg.setWindowModality(Qt::ApplicationModal);
    auto user = new QLineEdit(userValue, &dlg);
    auto pass = new QLineEdit(passValue, &dlg);
    pass->setEchoMode(QLineEdit::Password);
    auto host = new QLineEdit(hostValue, &dlg);
    auto port = new QLineEdit(QString::number(portValue), &dlg);
    QLabel *errorLabel = new QLabel(error, &dlg);
    errorLabel->setStyleSheet("color:red");
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    auto layout = new QFormLayout(&dlg);
    layout->addRow(errorLabel);
    layout->addRow("Host", host);
    layout->addRow("Port", port);
    layout->addRow("User", user);
    layout->addRow("Password", pass);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    dlg.show();
    dlg.raise();
    dlg.activateWindow();
    if (dlg.exec() != QDialog::Accepted) {
        return false;
    }

    hostValue = host->text();
    portValue = static_cast<quint16>(port->text().toUShort());
    userValue = user->text();
    passValue = pass->text();
    return true;
}

bool ChatWindow::openShareDialog(int &fpsOut, QSize &resolutionOut, int &qualityOut) {
    QDialog dlg(this);
    dlg.setWindowTitle("Share Screen Settings");
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
    watchButton->setText("Watch");
    watchingRemote = false;
    streamSelector->setCurrentIndex(0);
    sharePreview->setText("Screen preview");
    hide();

    buffer.clear();
    socket.disconnectFromHost();
    socket.connectToHost(hostValue, portValue);
}

void ChatWindow::setLoggedInUi(bool enabled) {
    sendButton->setEnabled(enabled);
    micToggleButton->setEnabled(enabled);
    shareToggleButton->setEnabled(enabled);
    watchButton->setEnabled(enabled);
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
}
