#pragma once

#include <QObject>
#include <QImage>
#include <QByteArray>
#include <QSize>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
}

class H264Encoder : public QObject {
    Q_OBJECT
public:
    H264Encoder();
    ~H264Encoder();
    bool init(int width, int height, int fps, int bitrate = 2000000);
    QByteArray encode(const QImage &image, quint32 &frameIdOut);
    void setSize(const QSize &size) { targetSize = size; }
    void setFps(int fps) { targetFps = fps; }
    void setBitrate(int bitrate) { targetBitrate = bitrate; }
    QByteArray config() const { return extraData; }

private:
    AVCodecContext *ctx = nullptr;
    AVFrame *frame = nullptr;
    SwsContext *sws = nullptr;
    int64_t pts = 0;
    QSize targetSize;
    int targetFps = 30;
    quint32 frameId = 0;
    int targetBitrate = 2000000;
    int srcW = 0;
    int srcH = 0;
    QByteArray extraData;
    bool open();
};
