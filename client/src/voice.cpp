#include "voice.h"
#include <QDataStream>
#include <QDebug>

namespace {
QAudioFormat chooseFormat(const QAudioDevice &device, const QAudioFormat &hint = QAudioFormat()) {
    QList<QAudioFormat> candidates;
    if (hint.isValid()) candidates << hint;
    candidates << device.preferredFormat();
    // Common mono/stereo formats
    QAudioFormat mono48;
    mono48.setSampleRate(48000);
    mono48.setChannelCount(1);
    mono48.setSampleFormat(QAudioFormat::Int16);
    QAudioFormat mono441 = mono48;
    mono441.setSampleRate(44100);
    QAudioFormat stereo48 = mono48;
    stereo48.setChannelCount(2);
    QAudioFormat stereo441 = mono441;
    stereo441.setChannelCount(2);
    candidates << mono48 << mono441 << stereo48 << stereo441;

    for (const QAudioFormat &f : candidates) {
        if (f.isValid() && device.isFormatSupported(f)) {
            return f;
        }
    }
    return QAudioFormat();
}

QByteArray convertChannels(const QByteArray &pcm, const QAudioFormat &from, const QAudioFormat &to) {
    if (from.sampleFormat() != QAudioFormat::Int16 || to.sampleFormat() != QAudioFormat::Int16) return pcm;
    if (from.channelCount() == to.channelCount()) return pcm;
    if (from.sampleRate() != to.sampleRate()) return pcm; // skip resample here

    QByteArray out;
    const qint16 *samples = reinterpret_cast<const qint16 *>(pcm.constData());
    const int frameCount = pcm.size() / static_cast<int>(sizeof(qint16) * from.channelCount());

    if (from.channelCount() == 1 && to.channelCount() == 2) {
        out.resize(frameCount * 2 * static_cast<int>(sizeof(qint16)));
        qint16 *dst = reinterpret_cast<qint16 *>(out.data());
        for (int i = 0; i < frameCount; ++i) {
            const qint16 s = samples[i];
            dst[2 * i] = s;
            dst[2 * i + 1] = s;
        }
    } else if (from.channelCount() == 2 && to.channelCount() == 1) {
        out.resize(frameCount * static_cast<int>(sizeof(qint16)));
        qint16 *dst = reinterpret_cast<qint16 *>(out.data());
        for (int i = 0; i < frameCount; ++i) {
            const int idx = i * 2;
            dst[i] = static_cast<qint16>((static_cast<int>(samples[idx]) + static_cast<int>(samples[idx + 1])) / 2);
        }
    } else {
        return pcm; // unsupported channel config
    }
    return out;
}

QByteArray convertSampleFormat(const QByteArray &pcm, const QAudioFormat &from, const QAudioFormat &to) {
    if (from.sampleFormat() == to.sampleFormat()) return pcm;
    if (from.channelCount() != to.channelCount() || from.sampleRate() != to.sampleRate()) {
        return pcm; // handled elsewhere
    }
    QByteArray out;
    if (from.sampleFormat() == QAudioFormat::Float && to.sampleFormat() == QAudioFormat::Int16) {
        const int sampleCount = pcm.size() / static_cast<int>(sizeof(float));
        out.resize(sampleCount * static_cast<int>(sizeof(qint16)));
        const float *src = reinterpret_cast<const float *>(pcm.constData());
        qint16 *dst = reinterpret_cast<qint16 *>(out.data());
        for (int i = 0; i < sampleCount; ++i) {
            float v = src[i];
            if (v > 1.0f) v = 1.0f;
            if (v < -1.0f) v = -1.0f;
            dst[i] = static_cast<qint16>(std::lround(v * 32767.0f));
        }
    } else if (from.sampleFormat() == QAudioFormat::Int16 && to.sampleFormat() == QAudioFormat::Float) {
        const int sampleCount = pcm.size() / static_cast<int>(sizeof(qint16));
        out.resize(sampleCount * static_cast<int>(sizeof(float)));
        const qint16 *src = reinterpret_cast<const qint16 *>(pcm.constData());
        float *dst = reinterpret_cast<float *>(out.data());
        for (int i = 0; i < sampleCount; ++i) {
            dst[i] = src[i] / 32768.0f;
        }
    } else {
        return pcm;
    }
    return out;
}

QByteArray resampleInt16(const QByteArray &pcm, int fromRate, int toRate, int channels) {
    if (fromRate == toRate) return pcm;
    if (channels <= 0) return pcm;
    const qint16 *src = reinterpret_cast<const qint16 *>(pcm.constData());
    const int frameCount = pcm.size() / static_cast<int>(sizeof(qint16) * channels);
    if (frameCount == 0) return pcm;
    const double ratio = static_cast<double>(toRate) / static_cast<double>(fromRate);
    const int outFrames = static_cast<int>(std::ceil(frameCount * ratio));
    QByteArray out;
    out.resize(outFrames * channels * static_cast<int>(sizeof(qint16)));
    qint16 *dst = reinterpret_cast<qint16 *>(out.data());
    for (int i = 0; i < outFrames; ++i) {
        const double srcPos = i / ratio;
        const int idx = static_cast<int>(srcPos);
        const double frac = srcPos - idx;
        for (int ch = 0; ch < channels; ++ch) {
            const int idx0 = std::min(idx, frameCount - 1) * channels + ch;
            const int idx1 = std::min(idx + 1, frameCount - 1) * channels + ch;
            const double v0 = src[idx0];
            const double v1 = src[idx1];
            const double interp = v0 + (v1 - v0) * frac;
            dst[i * channels + ch] = static_cast<qint16>(std::clamp(static_cast<int>(std::lround(interp)), -32768, 32767));
        }
    }
    return out;
}
}

Voice::Voice(QObject *parent) : QObject(parent) {}

bool Voice::startVoiceTransmission() {
    QAudioDevice device = inputDev.isNull() ? QMediaDevices::defaultAudioInput() : inputDev;
    format = chooseFormat(device, format);
    if (!format.isValid()) {
        qWarning() << "Input device does not support preferred/44.1k/48k 16bit mono:" << device.description();
        return false;
    }
    setAudioFormat(format);

    if (audioSource) {
        audioSource->stop();
        audioSource->deleteLater();
    }
    audioSource = new QAudioSource(device, format, this);
    audioSource->setVolume(inputVolume);
    connect(audioSource, &QAudioSource::stateChanged, this, &Voice::handleStateChanged);
    inputDevice = audioSource->start();
    if (inputDevice) {
        connect(inputDevice, &QIODevice::readyRead, this, &Voice::handleReadyRead);
        return true;
    } else {
        qWarning() << "Audio input start failed";
        audioSource->deleteLater();
        audioSource = nullptr;
        return false;
    }
}

void Voice::stopVoiceTransmission() {
    if (audioSource) {
        audioSource->stop();
        audioSource->deleteLater();
        audioSource = nullptr;
        inputDevice = nullptr;
    }
}

void Voice::handleStateChanged(QAudio::State state) {
    if (!audioSource) return;
    if (state == QAudio::StoppedState && audioSource->error() != QAudio::NoError) {
        qWarning() << "Audio input error:" << audioSource->error();
    }
}

void Voice::handleReadyRead() {
    if (!inputDevice) return;
    const QByteArray chunk = inputDevice->readAll();
    if (!chunk.isEmpty()) emit audioChunkReady(chunk);
}

void Voice::playReceivedAudio(const QByteArray &audioData) {
    if (!playbackEnabled) return;
    if (audioData.isEmpty()) return;
    // Try to use capture format; otherwise use preferred or 44.1k/48k fallback.
    if (!format.isValid()) {
        format.setSampleRate(48000);
        format.setChannelCount(1);
        format.setSampleFormat(QAudioFormat::Int16);
    }
    QAudioDevice outDev = outputDev.isNull() ? QMediaDevices::defaultAudioOutput() : outputDev;
    QByteArray dataToPlay = audioData;
    QAudioFormat playFormat = format;
    if (!outDev.isFormatSupported(playFormat)) {
        QAudioFormat f = chooseFormat(outDev, playFormat);
        if (!f.isValid()) {
            qWarning() << "Output device does not support preferred/44.1k/48k 16bit mono/stereo:" << outDev.description();
            return;
        }
        playFormat = f;
    }
    // Convert sample format if needed
    if (playFormat.sampleFormat() != format.sampleFormat() && format.sampleFormat() != QAudioFormat::Unknown) {
        dataToPlay = convertSampleFormat(dataToPlay, format, playFormat);
    }
    // Resample if needed (int16 only)
    const int srcChannels = format.channelCount() > 0 ? format.channelCount() : playFormat.channelCount();
    if (format.sampleFormat() == QAudioFormat::Int16 && playFormat.sampleFormat() == QAudioFormat::Int16 &&
        playFormat.sampleRate() != format.sampleRate()) {
        dataToPlay = resampleInt16(dataToPlay, format.sampleRate(), playFormat.sampleRate(), srcChannels);
    }
    // Convert channels if needed
    if (playFormat.sampleFormat() == QAudioFormat::Int16 && playFormat.channelCount() != srcChannels && srcChannels > 0) {
        QAudioFormat srcFmt = playFormat;
        srcFmt.setChannelCount(srcChannels);
        dataToPlay = convertChannels(dataToPlay, srcFmt, playFormat);
    }
    if (!audioSink || audioSink->format() != playFormat) {
        if (audioSink) {
            audioSink->stop();
            audioSink->deleteLater();
        }
        audioSink = new QAudioSink(outDev, playFormat, this);
        audioSink->setBufferSize(16384); // add headroom to smooth jittery UDP delivery
        audioSink->setVolume(outputVolume);
        outputDevice = audioSink->start();
    }
    if (!outputDevice && audioSink) {
        outputDevice = audioSink->start();
    }
    if (outputDevice) {
        outputDevice->write(dataToPlay);
    } else {
        emit playbackError("Audio output start failed");
    }
}

void Voice::setInputVolume(qreal volume) {
    if (audioSource) {
        audioSource->setVolume(volume);
    }
    inputVolume = volume;
}

void Voice::setOutputVolume(qreal volume) {
    if (audioSink) {
        audioSink->setVolume(volume);
    }
    outputVolume = volume;
}

void Voice::setInputDevice(const QAudioDevice &device) {
    inputDev = device;
    if (audioSource) {
        stopVoiceTransmission();
    }
}

void Voice::setOutputDevice(const QAudioDevice &device) {
    outputDev = device;
    if (audioSink) {
        audioSink->stop();
        audioSink->deleteLater();
        audioSink = nullptr;
        outputDevice = nullptr;
    }
}

bool Voice::restartInput() {
    stopVoiceTransmission();
    return startVoiceTransmission();
}

bool Voice::restartOutput() {
    if (audioSink) {
        audioSink->stop();
        audioSink->deleteLater();
        audioSink = nullptr;
        outputDevice = nullptr;
    }
    // Lazy recreate on first playReceivedAudio
    return true;
}
