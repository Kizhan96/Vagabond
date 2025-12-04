#include "screen_share.h"

#include <QApplication>
#include <QScreen>
#include <QDebug>
#include <mutex>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

struct FfmpegGrabber::CaptureContext {
    AVFormatContext *formatCtx = nullptr;
    AVCodecContext *codecCtx = nullptr;
    AVFrame *frame = nullptr;
    AVPacket *packet = nullptr;
    SwsContext *sws = nullptr;
    int videoStream = -1;
};

static int interruptCallback(void *opaque) {
    auto grabber = reinterpret_cast<FfmpegGrabber*>(opaque);
    return grabber ? grabber->stopRequested() : 0;
}

FfmpegGrabber::FfmpegGrabber(QObject *parent) : QObject(parent) {
    static std::once_flag once;
    std::call_once(once, []() {
        avdevice_register_all();
        av_log_set_level(AV_LOG_ERROR);
    });
}

FfmpegGrabber::~FfmpegGrabber() {
    cleanup();
}

bool FfmpegGrabber::setup(int fps, const QSize &target) {
    stopFlag = false;
    targetFps = fps > 0 ? fps : 10;
    targetSize = target;
    cleanup();
    return openInput();
}

bool FfmpegGrabber::openInput() {
    QSize screenSize;
    int offsetX = 0;
    int offsetY = 0;
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect geo = screen->geometry();
        const qreal dpr = screen->devicePixelRatio();
        screenSize = QSize(qRound(geo.width() * dpr), qRound(geo.height() * dpr));
        offsetX = qRound(geo.x() * dpr);
        offsetY = qRound(geo.y() * dpr);
    }
    if (!screenSize.isValid() || screenSize.isEmpty()) {
        screenSize = QSize(1920, 1080);
    }
    const QSize captureSize = screenSize;
    const int outW = (targetSize.isValid() && targetSize.width() > 0) ? targetSize.width() : captureSize.width();
    const int outH = (targetSize.isValid() && targetSize.height() > 0) ? targetSize.height() : captureSize.height();

    ctx = new CaptureContext();

    AVDictionary *opts = nullptr;
    av_dict_set(&opts, "framerate", QByteArray::number(targetFps).constData(), 0);
    const QByteArray sizeStr = QByteArray::number(captureSize.width()) + "x" + QByteArray::number(captureSize.height());
    av_dict_set(&opts, "video_size", sizeStr.constData(), 0);
    av_dict_set(&opts, "offset_x", QByteArray::number(offsetX).constData(), 0);
    av_dict_set(&opts, "offset_y", QByteArray::number(offsetY).constData(), 0);

    const AVInputFormat *inputFmt = av_find_input_format("gdigrab");
    if (!inputFmt) {
        emit error("FFmpeg: gdigrab input not available");
        av_dict_free(&opts);
        cleanup();
        return false;
    }

    ctx->formatCtx = avformat_alloc_context();
    ctx->formatCtx->interrupt_callback.callback = interruptCallback;
    ctx->formatCtx->interrupt_callback.opaque = this;

    if (avformat_open_input(&ctx->formatCtx, "desktop", inputFmt, &opts) < 0) {
        av_dict_free(&opts);
        emit error("FFmpeg: unable to open desktop capture");
        cleanup();
        return false;
    }
    av_dict_free(&opts);
    if (avformat_find_stream_info(ctx->formatCtx, nullptr) < 0) {
        emit error("FFmpeg: failed to read stream info");
        cleanup();
        return false;
    }

    // Find video stream
    for (unsigned i = 0; i < ctx->formatCtx->nb_streams; ++i) {
        if (ctx->formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            ctx->videoStream = static_cast<int>(i);
            break;
        }
    }
    if (ctx->videoStream < 0) {
        emit error("FFmpeg: no video stream");
        cleanup();
        return false;
    }

    AVCodecParameters *par = ctx->formatCtx->streams[ctx->videoStream]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(par->codec_id);
    if (!codec) {
        emit error("FFmpeg: decoder not found");
        cleanup();
        return false;
    }
    ctx->codecCtx = avcodec_alloc_context3(codec);
    if (!ctx->codecCtx) {
        cleanup();
        return false;
    }
    if (avcodec_parameters_to_context(ctx->codecCtx, par) < 0) {
        cleanup();
        return false;
    }
    if (avcodec_open2(ctx->codecCtx, codec, nullptr) < 0) {
        emit error("FFmpeg: failed to open decoder");
        cleanup();
        return false;
    }

    ctx->frame = av_frame_alloc();
    ctx->packet = av_packet_alloc();

    ctx->sws = sws_getContext(ctx->codecCtx->width, ctx->codecCtx->height, ctx->codecCtx->pix_fmt,
                              outW, outH,
                              AV_PIX_FMT_BGRA, SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!ctx->sws) {
        emit error("FFmpeg: failed to init scaler");
        cleanup();
        return false;
    }
    return true;
}

void FfmpegGrabber::runLoop() {
    if (!ctx) {
        emit stopped();
        return;
    }
    emit started();
    const int sleepMs = qMax(1, 1000 / targetFps);
    while (!stopFlag.load()) {
        if (av_read_frame(ctx->formatCtx, ctx->packet) < 0) {
            QThread::msleep(static_cast<unsigned long>(sleepMs));
            continue;
        }
        if (ctx->packet->stream_index != ctx->videoStream) {
            av_packet_unref(ctx->packet);
            continue;
        }
        if (avcodec_send_packet(ctx->codecCtx, ctx->packet) == 0) {
            while (avcodec_receive_frame(ctx->codecCtx, ctx->frame) == 0) {
                int dstW = targetSize.isValid() ? targetSize.width() : ctx->frame->width;
                int dstH = targetSize.isValid() ? targetSize.height() : ctx->frame->height;
                uint8_t *dstData[4] = {nullptr};
                int dstLinesize[4] = {0};
                if (av_image_alloc(dstData, dstLinesize, dstW, dstH, AV_PIX_FMT_BGRA, 1) < 0) {
                    continue;
                }
                sws_scale(ctx->sws, ctx->frame->data, ctx->frame->linesize, 0, ctx->frame->height, dstData, dstLinesize);
                QImage img(dstData[0], dstW, dstH, dstLinesize[0], QImage::Format_ARGB32);
                emit frameReady(img.copy()); // copy before freeing buffer
                av_free(dstData[0]);
            }
        }
        av_packet_unref(ctx->packet);
        QThread::msleep(static_cast<unsigned long>(sleepMs));
    }
    cleanup();
    emit stopped();
}

void FfmpegGrabber::requestStop() {
    stopFlag = true;
}

bool FfmpegGrabber::stopRequested() const {
    return stopFlag.load();
}

void FfmpegGrabber::cleanup() {
    if (!ctx) return;
    if (ctx->packet) {
        av_packet_free(&ctx->packet);
    }
    if (ctx->frame) {
        av_frame_free(&ctx->frame);
    }
    if (ctx->codecCtx) {
        avcodec_free_context(&ctx->codecCtx);
    }
    if (ctx->formatCtx) {
        avformat_close_input(&ctx->formatCtx);
        avformat_free_context(ctx->formatCtx);
        ctx->formatCtx = nullptr;
    }
    if (ctx->sws) {
        sws_freeContext(ctx->sws);
    }
    delete ctx;
    ctx = nullptr;
}

ScreenShare::ScreenShare(QObject *parent) : QObject(parent) {
    connect(&fallbackTimer, &QTimer::timeout, this, &ScreenShare::captureFallback);
    fallbackTimer.setTimerType(Qt::PreciseTimer);
    currentFps = 10;
}

ScreenShare::~ScreenShare() {
    stopCapturing();
}

void ScreenShare::startCapturing(int intervalMs) {
    if (ffmpegRunning || fallbackTimer.isActive()) return;
    intervalMs = qMax(33, intervalMs); // cap at ~30fps max speed
    this->intervalMs = intervalMs;
    currentFps = qMax(10, 1000 / intervalMs);
    if (startFfmpeg()) {
        return;
    }
    startFallbackTimer();
}

void ScreenShare::stopCapturing() {
    const bool wasFallback = fallbackTimer.isActive();
    const bool wasFfmpeg = ffmpegRunning;
    stopFfmpeg();
    stopFallbackTimer();
    if (wasFallback && !wasFfmpeg) {
        emit stopped();
    }
}

bool ScreenShare::startFfmpeg() {
    if (ffmpegRunning) return true;
    grabber = new FfmpegGrabber();
    grabber->moveToThread(&captureThread);
    connect(&captureThread, &QThread::finished, grabber, &QObject::deleteLater);
    connect(grabber, &FfmpegGrabber::frameReady, this, [this](const QImage &img) {
        emit frameReady(QPixmap::fromImage(img));
    });
    connect(grabber, &FfmpegGrabber::started, this, &ScreenShare::started);
    connect(grabber, &FfmpegGrabber::stopped, this, [this]() {
        ffmpegRunning = false;
    });
    connect(grabber, &FfmpegGrabber::error, this, &ScreenShare::error);

    captureThread.start();

    bool ok = false;
    QMetaObject::invokeMethod(grabber, "setup", Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(bool, ok),
                              Q_ARG(int, currentFps),
                              Q_ARG(QSize, targetSize));
    if (!ok) {
        captureThread.quit();
        captureThread.wait();
        grabber = nullptr;
        return false;
    }
    ffmpegRunning = true;
    connect(grabber, &FfmpegGrabber::stopped, this, &ScreenShare::stopped);
    QMetaObject::invokeMethod(grabber, "runLoop", Qt::QueuedConnection);
    return true;
}

void ScreenShare::stopFfmpeg() {
    if (!ffmpegRunning && !grabber) return;
    if (grabber) {
        // No event loop in captureThread, use direct call to flip atomic flag.
        QMetaObject::invokeMethod(grabber, "requestStop", Qt::DirectConnection);
    }
    if (captureThread.isRunning()) {
        if (!captureThread.wait(500)) {
            captureThread.requestInterruption();
            captureThread.wait(200);
        }
    }
    grabber = nullptr;
    ffmpegRunning = false;
}

void ScreenShare::startFallbackTimer() {
    if (fallbackTimer.isActive()) return;
    fallbackTimer.start(intervalMs);
    emit started();
}

void ScreenShare::stopFallbackTimer() {
    if (!fallbackTimer.isActive()) return;
    fallbackTimer.stop();
}

void ScreenShare::captureFallback() {
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) return;
    emit frameReady(screen->grabWindow(0));
}

void ScreenShare::setFps(int fps) {
    if (fps <= 0) return;
    currentFps = fps;
    intervalMs = qMax(10, 1000 / fps);
    if (fallbackTimer.isActive()) {
        fallbackTimer.start(intervalMs);
    }
}

void ScreenShare::setTargetSize(const QSize &size) {
    if (size.isValid()) {
        targetSize = size;
    }
}
