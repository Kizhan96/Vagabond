#ifndef VOICE_H
#define VOICE_H

#include <QObject>
#include <QAudio>
#include <QAudioFormat>
#include <QAudioSource>
#include <QAudioSink>
#include <QByteArray>
#include <QMediaDevices>

class Voice : public QObject {
    Q_OBJECT

public:
    explicit Voice(QObject *parent = nullptr);
    ~Voice() override = default;

    bool startVoiceTransmission();
    void stopVoiceTransmission();
    void playReceivedAudio(const QByteArray &audioData);
    void setPlaybackEnabled(bool enabled) { playbackEnabled = enabled; }
    void setInputVolume(qreal volume);  // 0.0 - 1.0
    void setOutputVolume(qreal volume); // 0.0 - 1.0
    bool isCapturing() const { return audioSource && audioSource->state() == QAudio::ActiveState; }
    QAudioFormat audioFormat() const { return format; }
    void setVolumes(qreal inVol, qreal outVol) { inputVolume = inVol; outputVolume = outVol; }

signals:
    void audioChunkReady(const QByteArray &data);
    void playbackError(const QString &message);

private slots:
    void handleStateChanged(QAudio::State state);
    void handleReadyRead();

private:
    QAudioSource *audioSource = nullptr;
    QAudioSink *audioSink = nullptr;
    QIODevice *inputDevice = nullptr;
    QIODevice *outputDevice = nullptr;
    QAudioFormat format;
    bool playbackEnabled = true;
    qreal inputVolume = 1.0;
    qreal outputVolume = 1.0;
};

#endif // VOICE_H
