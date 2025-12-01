#include "screen_share.h"

#include <QApplication>
#include <QScreen>

ScreenShare::ScreenShare(QObject *parent) : QObject(parent) {
    connect(&timer, &QTimer::timeout, this, &ScreenShare::captureFrame);
}

void ScreenShare::startCapturing(int intervalMs) {
    if (timer.isActive()) return;
    intervalMs = qMax(33, intervalMs); // cap at ~30fps max speed
    this->intervalMs = intervalMs;
    timer.start(this->intervalMs);
    emit started();
}

void ScreenShare::stopCapturing() {
    if (!timer.isActive()) return;
    timer.stop();
    emit stopped();
}

void ScreenShare::captureFrame() {
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) return;
    const QPixmap frame = screen->grabWindow(0);
    emit frameReady(frame);
}

void ScreenShare::setFps(int fps) {
    if (fps <= 0) return;
    intervalMs = qMax(10, 1000 / fps);
    if (timer.isActive()) {
        timer.start(intervalMs);
    }
}
