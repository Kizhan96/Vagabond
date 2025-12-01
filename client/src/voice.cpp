#include "voice.h"
#include <QDataStream>
#include <QDebug>

Voice::Voice(QObject *parent) : QObject(parent) {}

bool Voice::startVoiceTransmission() {
    QAudioDevice device = QMediaDevices::defaultAudioInput();
    // Try preferred; if not supported, iterate fallback list.
    QList<QAudioFormat> candidates;
    candidates << device.preferredFormat();
    QAudioFormat fallback;
    fallback.setSampleRate(44100);
    fallback.setChannelCount(1);
    fallback.setSampleFormat(QAudioFormat::Int16);
    candidates << fallback;
    fallback.setSampleRate(48000);
    candidates << fallback;

    bool supported = false;
    for (const QAudioFormat &f : candidates) {
        if (f.isValid() && device.isFormatSupported(f)) {
            format = f;
            supported = true;
            break;
        }
    }
    if (!supported) {
        qWarning() << "Input device does not support preferred/44.1k/48k 16bit mono:" << device.description();
        return false;
    }

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
        format.setSampleRate(44100);
        format.setChannelCount(1);
        format.setSampleFormat(QAudioFormat::Int16);
    }
    QAudioDevice outDev = QMediaDevices::defaultAudioOutput();
    if (!outDev.isFormatSupported(format)) {
        QList<QAudioFormat> outs;
        outs << outDev.preferredFormat();
        QAudioFormat f441;
        f441.setSampleRate(44100);
        f441.setChannelCount(1);
        f441.setSampleFormat(QAudioFormat::Int16);
        QAudioFormat f48 = f441;
        f48.setSampleRate(48000);
        outs << f441 << f48;
        bool ok = false;
        for (const QAudioFormat &f : outs) {
            if (f.isValid() && outDev.isFormatSupported(f)) {
                format = f;
                ok = true;
                break;
            }
        }
        if (!ok) {
            qWarning() << "Output device does not support preferred/44.1k/48k 16bit mono:" << outDev.description();
            return;
        }
    }
    if (!audioSink) {
        audioSink = new QAudioSink(outDev, format, this);
        audioSink->setVolume(outputVolume);
    }
    if (!outputDevice) {
        outputDevice = audioSink->start();
    }
    if (outputDevice) {
        outputDevice->write(audioData);
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
