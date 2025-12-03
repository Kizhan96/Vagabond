#pragma once

#include <QObject>
#include <QImage>
#include <QByteArray>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

class H264Decoder : public QObject {
    Q_OBJECT
public:
    H264Decoder();
    ~H264Decoder();
    bool init();
    void setConfig(const QByteArray &cfg) { config = cfg; }
    QImage decode(const QByteArray &data);

private:
    AVCodecContext *ctx = nullptr;
    AVFrame *frame = nullptr;
    SwsContext *sws = nullptr;
    QByteArray config;
    bool open();
};
