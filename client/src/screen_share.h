#ifndef SCREEN_SHARE_H
#define SCREEN_SHARE_H

#include <QObject>
#include <QPixmap>
#include <QTimer>

class ScreenShare : public QObject {
    Q_OBJECT
public:
    explicit ScreenShare(QObject *parent = nullptr);

    void startCapturing(int intervalMs = 200);
    void stopCapturing();
    bool isCapturing() const { return timer.isActive(); }
    void setFps(int fps);

signals:
    void frameReady(const QPixmap &frame);
    void started();
    void stopped();

private slots:
    void captureFrame();

private:
    QTimer timer;
    int intervalMs = 200;
};

#endif // SCREEN_SHARE_H
