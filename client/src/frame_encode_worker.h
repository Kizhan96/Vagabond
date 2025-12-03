#ifndef FRAME_ENCODE_WORKER_H
#define FRAME_ENCODE_WORKER_H

#include <QObject>
#include <QImage>
#include <QSize>
#include <QByteArray>
#include "h264_encoder.h"

class FrameEncodeWorker : public QObject {
    Q_OBJECT
public:
    void setEncoder(H264Encoder *enc) { encoder = enc; }
public slots:
    void encodeFrame(const QImage &frame, const QSize &targetSize, int quality, quint32 frameId, qint64 timestampMs);

signals:
    void encodedFrameReady(const QByteArray &data, quint32 frameId, qint64 timestampMs);

private:
    H264Encoder *encoder = nullptr;
};

#endif // FRAME_ENCODE_WORKER_H
