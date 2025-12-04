#ifndef SCREEN_SHARE_H
#define SCREEN_SHARE_H

#include <QObject>
#include <QPixmap>
#include <QTimer>
#include <QThread>
#include <QImage>
#include <QSize>
#include <atomic>

class FfmpegGrabber : public QObject {
    Q_OBJECT
public:
    explicit FfmpegGrabber(QObject *parent = nullptr);
    ~FfmpegGrabber();

public slots:
    bool setup(int fps, const QSize &target);
    void runLoop();
    void requestStop();
    bool stopRequested() const;

signals:
    void frameReady(const QImage &frame);
    void started();
    void stopped();
    void error(const QString &msg);

private:
    bool openInput();
    void cleanup();

    QSize targetSize;
    int targetFps = 10;
    std::atomic<bool> stopFlag{false};
    struct CaptureContext;
    CaptureContext *ctx = nullptr;
};

class ScreenShare : public QObject {
    Q_OBJECT
public:
    explicit ScreenShare(QObject *parent = nullptr);
    ~ScreenShare() override;

    void startCapturing(int intervalMs = 200);
    void stopCapturing();
    bool isCapturing() const { return ffmpegRunning || fallbackTimer.isActive(); }
    void setFps(int fps);
    void setTargetSize(const QSize &size);

signals:
    void frameReady(const QPixmap &frame);
    void started();
    void stopped();
    void error(const QString &msg);

private slots:
    void captureFallback();

private:
    bool startFfmpeg();
    void stopFfmpeg();
    void startFallbackTimer();
    void stopFallbackTimer();

    QTimer fallbackTimer;
    int intervalMs = 200;
    int currentFps = 5;
    QSize targetSize;
    QThread captureThread;
    FfmpegGrabber *grabber = nullptr;
    bool ffmpegRunning = false;
};

#endif // SCREEN_SHARE_H
