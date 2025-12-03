#include "screen_share.h"

#include <QApplication>
#include <QScreen>

ScreenShare::ScreenShare(QObject *parent) : QObject(parent) {
    connect(&timer, &QTimer::timeout, this, &ScreenShare::captureFrame);
    timer.setTimerType(Qt::PreciseTimer);
    dxgiReady = dxgi.initialize();
}

void ScreenShare::startCapturing(int intervalMs) {
    if (timer.isActive()) return;
    intervalMs = qMax(33, intervalMs); // cap at ~30fps max speed
    this->intervalMs = intervalMs;
    if (!dxgiReady) {
        dxgiReady = dxgi.initialize();
        if (!dxgiReady) {
            emit stopped();
            return;
        }
    }
    timer.start(this->intervalMs);
    emit started();
}

void ScreenShare::stopCapturing() {
    if (!timer.isActive()) return;
    timer.stop();
    emit stopped();
}

void ScreenShare::captureFrame() {
    if (!dxgiReady) {
        dxgiReady = dxgi.initialize();
    }
    QImage img;
    if (dxgiReady) {
        img = dxgi.grab();
        if (img.isNull()) {
            dxgiReady = false;
        }
    }
    if (img.isNull()) {
        // fallback to Qt grab to avoid black screen
        QScreen *screen = QGuiApplication::primaryScreen();
        if (!screen) return;
        emit frameReady(screen->grabWindow(0));
    } else {
        emit frameReady(QPixmap::fromImage(img));
    }
}

void ScreenShare::setFps(int fps) {
    if (fps <= 0) return;
    intervalMs = qMax(10, 1000 / fps);
    if (timer.isActive()) {
        timer.start(intervalMs);
    }
}

void ScreenShare::setTargetSize(const QSize &size) {
    if (size.isValid()) {
        dxgi.resize(size.width(), size.height());
    }
}
