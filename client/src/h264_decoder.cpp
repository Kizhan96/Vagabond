#include "h264_decoder.h"
#include <QDebug>
extern "C" {
#include <libavutil/imgutils.h>
}

H264Decoder::H264Decoder() {}

H264Decoder::~H264Decoder() {
    if (sws) sws_freeContext(sws);
    if (frame) av_frame_free(&frame);
    if (ctx) avcodec_free_context(&ctx);
}

bool H264Decoder::open() {
    const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) return false;
    ctx = avcodec_alloc_context3(codec);
    if (!ctx) return false;
    if (avcodec_open2(ctx, codec, nullptr) < 0) return false;
    frame = av_frame_alloc();
    return frame != nullptr;
}

bool H264Decoder::init() {
    return open();
}

QImage H264Decoder::decode(const QByteArray &data) {
    if (!ctx && !open()) return QImage();
    QByteArray full = data;
    if (!config.isEmpty()) {
        full = config + data;
    }
    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = reinterpret_cast<uint8_t*>(const_cast<char*>(full.constData()));
    pkt.size = full.size();
    if (avcodec_send_packet(ctx, &pkt) < 0) return QImage();
    if (avcodec_receive_frame(ctx, frame) < 0) return QImage();

    sws = sws_getCachedContext(sws, frame->width, frame->height, static_cast<AVPixelFormat>(frame->format),
                               frame->width, frame->height, AV_PIX_FMT_RGBA,
                               SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws) return QImage();

    QImage img(frame->width, frame->height, QImage::Format_RGBA8888);
    uint8_t *dstData[4];
    int dstLinesize[4];
    av_image_fill_arrays(dstData, dstLinesize, img.bits(), AV_PIX_FMT_RGBA, img.width(), img.height(), 1);
    sws_scale(sws, frame->data, frame->linesize, 0, frame->height, dstData, dstLinesize);
    return img;
}
