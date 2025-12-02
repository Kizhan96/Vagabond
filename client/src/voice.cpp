#include "voice.h"
#include <QDataStream>
#include <QDebug>

namespace {
QAudioFormat chooseFormat(const QAudioDevice &device, const QAudioFormat &hint = QAudioFormat()) {
    QList<QAudioFormat> candidates;
    if (hint.isValid()) candidates << hint;
    candidates << device.preferredFormat();
    QAudioFormat mono441;
    mono441.setSampleRate(44100);
    mono441.setChannelCount(1);
    mono441.setSampleFormat(QAudioFormat::Int16);
    QAudioFormat mono48 = mono441;
    mono48.setSampleRate(48000);
    candidates << mono48 << mono441;

    for (const QAudioFormat &f : candidates) {
        if (f.isValid() && device.isFormatSupported(f)) {
            return f;
        }
    }
    return QAudioFormat();
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
    QAudioDevice outDev = outputDev.isNull() ? QMediaDevices::defaultAudioOutput() : outputDev;
    if (!outDev.isFormatSupported(format)) {
        QAudioFormat f = chooseFormat(outDev, format);
        if (!f.isValid()) {
            qWarning() << "Output device does not support preferred/44.1k/48k 16bit mono:" << outDev.description();
            return;
        }
        format = f;
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
