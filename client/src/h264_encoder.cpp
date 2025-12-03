#include "h264_encoder.h"
#include <QDebug>

H264Encoder::H264Encoder() {}

H264Encoder::~H264Encoder() {
    if (sws) sws_freeContext(sws);
    if (frame) av_frame_free(&frame);
    if (ctx) avcodec_free_context(&ctx);
}

bool H264Encoder::open() {
    static bool logLevelSet = false;
    if (!logLevelSet) {
        av_log_set_level(AV_LOG_ERROR); // silence encoder info/stats spam
        logLevelSet = true;
    }
    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) return false;
    ctx = avcodec_alloc_context3(codec);
    if (!ctx) return false;
    ctx->width = targetSize.width();
    ctx->height = targetSize.height();
    ctx->time_base = AVRational{1, targetFps};
    ctx->framerate = AVRational{targetFps, 1};
    ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    ctx->bit_rate = targetBitrate;
    ctx->gop_size = targetFps;
    ctx->max_b_frames = 0;
    av_opt_set(ctx->priv_data, "preset", "veryfast", 0);
    av_opt_set(ctx->priv_data, "tune", "zerolatency", 0);
    av_opt_set(ctx->priv_data, "x264-params", "log=0", 0);

    if (avcodec_open2(ctx, codec, nullptr) < 0) return false;

    frame = av_frame_alloc();
    frame->format = ctx->pix_fmt;
    frame->width = ctx->width;
    frame->height = ctx->height;
    if (av_frame_get_buffer(frame, 32) < 0) return false;

    if (ctx->extradata && ctx->extradata_size > 0) {
        extraData = QByteArray(reinterpret_cast<const char*>(ctx->extradata), ctx->extradata_size);
    }
    return true;
}

bool H264Encoder::init(int width, int height, int fps, int bitrate) {
    if (sws) { sws_freeContext(sws); sws = nullptr; }
    if (frame) { av_frame_free(&frame); frame = nullptr; }
    if (ctx) { avcodec_free_context(&ctx); ctx = nullptr; }
    targetSize = QSize(width, height);
    targetFps = fps;
    targetBitrate = bitrate;
    pts = 0;
    frameId = 0;
    extraData.clear();
    return open();
}

QByteArray H264Encoder::encode(const QImage &image, quint32 &frameIdOut) {
    if (!ctx && !open()) return {};
    if (image.isNull()) return {};

    QImage src = image.convertToFormat(QImage::Format_RGBA8888);
    if (src.size() != targetSize) {
        src = src.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    if (!sws || srcW != src.width() || srcH != src.height()) {
        srcW = src.width();
        srcH = src.height();
        if (sws) sws_freeContext(sws);
        sws = sws_getContext(srcW, srcH, AV_PIX_FMT_RGBA,
                             ctx->width, ctx->height, ctx->pix_fmt,
                             SWS_BILINEAR, nullptr, nullptr, nullptr);
    }

    uint8_t *srcData[4];
    int srcLinesize[4];
    av_image_fill_arrays(srcData, srcLinesize, src.bits(), AV_PIX_FMT_RGBA, src.width(), src.height(), 1);
    sws_scale(sws, srcData, srcLinesize, 0, src.height(), frame->data, frame->linesize);

    frame->pts = pts++;
    frameIdOut = ++frameId;

    QByteArray out;
    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = nullptr;
    pkt.size = 0;

    int ret = avcodec_send_frame(ctx, frame);
    if (ret < 0) return {};
    ret = avcodec_receive_packet(ctx, &pkt);
    if (ret == 0) {
        out = QByteArray(reinterpret_cast<const char*>(pkt.data), pkt.size);
        av_packet_unref(&pkt);
    }
    return out;
}
