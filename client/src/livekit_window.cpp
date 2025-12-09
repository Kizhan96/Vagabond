#include "livekit_window.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>
#include <QTextCursor>
#include "livekit_room_widget.h"

LiveKitWindow::LiveKitWindow(QWidget *parent) : QMainWindow(parent) {
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);

    auto *formLayout = new QHBoxLayout();
    urlInput = new QLineEdit(this);
    urlInput->setPlaceholderText(QStringLiteral("wss://YOUR-LIVEKIT/api"));
    urlInput->setText(QString::fromUtf8(qgetenv("LIVEKIT_URL")));
    tokenInput = new QLineEdit(this);
    tokenInput->setPlaceholderText(QStringLiteral("LIVEKIT_TOKEN"));
    tokenInput->setText(QString::fromUtf8(qgetenv("LIVEKIT_TOKEN")));
    roomInput = new QLineEdit(this);
    roomInput->setPlaceholderText(QStringLiteral("Room label"));
    roomInput->setText(QStringLiteral("General"));
    connectButton = new QPushButton(tr("Open room"), this);
    statusLabel = new QLabel(tr("Provide LiveKit URL, token and room label"), this);

    formLayout->addWidget(new QLabel(tr("Server"), this));
    formLayout->addWidget(urlInput, 2);
    formLayout->addWidget(new QLabel(tr("Token"), this));
    formLayout->addWidget(tokenInput, 2);
    formLayout->addWidget(new QLabel(tr("Room"), this));
    formLayout->addWidget(roomInput, 1);
    formLayout->addWidget(connectButton);

    tabWidget = new QTabWidget(this);
    tabWidget->setTabsClosable(true);

    logView = new QTextEdit(this);
    logView->setReadOnly(true);

    layout->addLayout(formLayout);
    layout->addWidget(statusLabel);
    layout->addWidget(tabWidget, 1);
    layout->addWidget(new QLabel(tr("Event log"), this));
    layout->addWidget(logView, 1);

    setCentralWidget(central);
    setWindowTitle(QStringLiteral("LiveKit Client"));
    resize(1200, 900);

    connect(connectButton, &QPushButton::clicked, this, &LiveKitWindow::connectToLiveKit);
    connect(tabWidget, &QTabWidget::tabCloseRequested, this, &LiveKitWindow::closeTab);
}

void LiveKitWindow::connectToLiveKit() {
    const QString url = urlInput->text().trimmed();
    const QString token = tokenInput->text().trimmed();
    const QString room = roomInput->text().trimmed();

    if (url.isEmpty() || token.isEmpty()) {
        statusLabel->setText(tr("URL and token are required"));
        appendLog(tr("Missing URL or token"));
        return;
    }

    const QString label = room.isEmpty() ? QStringLiteral("Room") : room;
    appendLog(tr("Opening LiveKit room %1").arg(label));

    auto *roomWidget = new LiveKitRoomWidget(url, token, label, this);
    const int idx = tabWidget->addTab(roomWidget, label);
    tabWidget->setCurrentIndex(idx);

    statusLabel->setText(tr("Connected tab count: %1").arg(tabWidget->count()));
}

void LiveKitWindow::closeTab(int index) {
    QWidget *widget = tabWidget->widget(index);
    tabWidget->removeTab(index);
    widget->deleteLater();
    statusLabel->setText(tr("Connected tab count: %1").arg(tabWidget->count()));
    appendLog(tr("Closed room tab %1").arg(index + 1));
}

void LiveKitWindow::appendLog(const QString &line) {
    logView->append(line);
    QTextCursor c = logView->textCursor();
    c.movePosition(QTextCursor::End);
    logView->setTextCursor(c);
}
