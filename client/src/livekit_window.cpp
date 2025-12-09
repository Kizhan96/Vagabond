#include "livekit_window.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QNetworkRequest>
#include <QVBoxLayout>
#include <QWidget>
#include "livekit_room_widget.h"

LiveKitWindow::LiveKitWindow(QWidget *parent) : QMainWindow(parent) {
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);

    auto *authLayout = new QHBoxLayout();
    const QString defaultAuthUrl = QString::fromUtf8(qgetenv("LIVEKIT_AUTH_URL"));
    authUrlInput = new QLineEdit(this);
    authUrlInput->setPlaceholderText(QStringLiteral("Auth URL"));
    authUrlInput->setText(defaultAuthUrl.isEmpty() ? QStringLiteral("https://livekit.vagabovnr.moscow/api/token")
                                                   : defaultAuthUrl);
    usernameInput = new QLineEdit(this);
    usernameInput->setPlaceholderText(QStringLiteral("login"));
    usernameInput->setText(QStringLiteral("test"));
    passwordInput = new QLineEdit(this);
    passwordInput->setPlaceholderText(QStringLiteral("password"));
    passwordInput->setEchoMode(QLineEdit::Password);
    passwordInput->setText(QStringLiteral("test"));
    roomInput = new QLineEdit(this);
    roomInput->setPlaceholderText(QStringLiteral("Room label"));
    roomInput->setText(QStringLiteral("general"));
    connectButton = new QPushButton(tr("Sign in & join"), this);
    statusLabel = new QLabel(tr("Enter login, password and room"), this);
    accountLabel = new QLabel(tr("Not signed in"), this);

    audioCheck = new QCheckBox(tr("Join with microphone on"), this);
    audioCheck->setChecked(true);
    videoCheck = new QCheckBox(tr("Join with camera on"), this);
    videoCheck->setChecked(true);

    authLayout->addWidget(new QLabel(tr("Auth URL"), this));
    authLayout->addWidget(authUrlInput, 2);
    authLayout->addWidget(new QLabel(tr("Login"), this));
    authLayout->addWidget(usernameInput, 1);
    authLayout->addWidget(new QLabel(tr("Password"), this));
    authLayout->addWidget(passwordInput, 1);
    authLayout->addWidget(new QLabel(tr("Room"), this));
    authLayout->addWidget(roomInput, 1);
    authLayout->addWidget(connectButton);

    tabWidget = new QTabWidget(this);
    tabWidget->setTabsClosable(true);

    layout->addLayout(authLayout);
    layout->addWidget(accountLabel);
    layout->addWidget(statusLabel);
    layout->addWidget(audioCheck);
    layout->addWidget(videoCheck);
    layout->addWidget(tabWidget, 1);

    setCentralWidget(central);
    setWindowTitle(QStringLiteral("LiveKit Client"));
    resize(1200, 900);

    connect(connectButton, &QPushButton::clicked, this, &LiveKitWindow::connectToLiveKit);
    connect(tabWidget, &QTabWidget::tabCloseRequested, this, &LiveKitWindow::closeTab);
}

void LiveKitWindow::connectToLiveKit() {
    if (pendingAuthReply) {
        pendingAuthReply->deleteLater();
        pendingAuthReply = nullptr;
    }

    const QString identity = usernameInput->text().trimmed();
    const QString password = passwordInput->text();
    QString room = roomInput->text().trimmed();

    if (identity.isEmpty()) {
        statusLabel->setText(tr("Login is required"));
        appendLog(tr("Missing login"));
        return;
    }

    if (room.isEmpty()) {
        room = QStringLiteral("general");
    }

    const QUrl endpoint = authEndpoint();
    if (!endpoint.isValid()) {
        statusLabel->setText(tr("Auth endpoint is invalid"));
        return;
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("identity"), identity);
    // Support servers that ignore passwords while keeping parity with older backends.
    if (!password.isEmpty()) {
        payload.insert(QStringLiteral("password"), password);
    }
    payload.insert(QStringLiteral("roomName"), room);
    payload.insert(QStringLiteral("room"), room);

    QNetworkRequest request(endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    setFormEnabled(false);
        statusLabel->setText(tr("Requesting LiveKit token…"));
    appendLog(tr("Contacting %1").arg(endpoint.toString()));
    pendingAuthReply = network.post(request, QJsonDocument(payload).toJson());
    lastIdentity = identity;

    connect(pendingAuthReply, &QNetworkReply::finished, this, &LiveKitWindow::handleAuthResponse);
}

void LiveKitWindow::handleAuthResponse() {
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) return;

    reply->deleteLater();
    if (reply == pendingAuthReply) {
        pendingAuthReply = nullptr;
    }

    setFormEnabled(true);

    const QByteArray data = reply->readAll();

    if (reply->error() != QNetworkReply::NoError) {
        const QVariant status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        QString errorText = reply->errorString();
        if (status.isValid()) {
            errorText = tr("HTTP %1: %2").arg(status.toInt()).arg(errorText);
        }

        const QString body = QString::fromUtf8(data).trimmed();
        if (!body.isEmpty()) {
            errorText.append(tr(" — %1").arg(body.left(512)));
        }

        statusLabel->setText(tr("Auth failed: %1").arg(errorText));
        appendLog(tr("Auth failed: %1").arg(errorText));
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        statusLabel->setText(tr("Unexpected response from server"));
        return;
    }

    const QJsonObject obj = doc.object();
    QString url = obj.value(QStringLiteral("livekitUrl")).toString();
    if (url.isEmpty()) {
        url = obj.value(QStringLiteral("url")).toString();
    }

    const QString token = obj.value(QStringLiteral("token")).toString();
    const QString room = obj.value(QStringLiteral("roomName"))
                             .toString(obj.value(QStringLiteral("room"))
                                           .toString(roomInput->text().trimmed()));

    if (url.isEmpty()) {
        url = QStringLiteral("wss://livekit.vagabovnr.moscow");
    }

    if (token.isEmpty()) {
        statusLabel->setText(tr("Server did not return a LiveKit token"));
        appendLog(tr("Missing token in response"));
        return;
    }

    accountLabel->setText(tr("Signed in as %1").arg(lastIdentity));
    appendLog(tr("Opening LiveKit room %1").arg(room));
    openRoomTab(url, token, room, audioCheck->isChecked(), videoCheck->isChecked());
}

void LiveKitWindow::closeTab(int index) {
    QWidget *widget = tabWidget->widget(index);
    tabWidget->removeTab(index);
    widget->deleteLater();
    statusLabel->setText(tr("Connected tab count: %1").arg(tabWidget->count()));
    appendLog(tr("Closed room tab %1").arg(index + 1));
}

void LiveKitWindow::appendLog(const QString &line) {
    statusLabel->setText(line);
}

void LiveKitWindow::setFormEnabled(bool enabled) {
    authUrlInput->setEnabled(enabled);
    usernameInput->setEnabled(enabled);
    passwordInput->setEnabled(enabled);
    roomInput->setEnabled(enabled);
    connectButton->setEnabled(enabled);
    audioCheck->setEnabled(enabled);
    videoCheck->setEnabled(enabled);
}

QUrl LiveKitWindow::authEndpoint() const {
    QString fromField = authUrlInput->text().trimmed();
    if (fromField.isEmpty()) {
        fromField = QStringLiteral("https://livekit.vagabovnr.moscow/api/token");
    }

    return QUrl(fromField);
}

void LiveKitWindow::openRoomTab(const QString &url, const QString &token, const QString &room,
                                bool startWithAudio, bool startWithVideo) {
    const QString label = room.isEmpty() ? QStringLiteral("Room") : room;
    auto *roomWidget = new LiveKitRoomWidget(url, token, label, startWithAudio, startWithVideo, this);
    const int idx = tabWidget->addTab(roomWidget, label);
    tabWidget->setCurrentIndex(idx);
    statusLabel->setText(tr("Connected tab count: %1").arg(tabWidget->count()));
}
