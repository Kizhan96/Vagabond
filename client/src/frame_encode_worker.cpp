#include "frame_encode_worker.h"
#include <QBuffer>

void FrameEncodeWorker::encodeFrame(const QImage &frame, const QSize &targetSize, int quality, quint32 frameId, qint64 timestampMs) {
    if (!encoder) return;
    QImage img = frame;
    if (targetSize.isValid() && targetSize.width() > 0 && targetSize.height() > 0) {
        img = frame.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    quint32 fid = frameId;
    QByteArray bytes = encoder->encode(img, fid);
    if (!bytes.isEmpty()) {
        emit encodedFrameReady(bytes, fid, timestampMs);
    }
}
