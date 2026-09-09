#pragma once
#include <QObject>
#include <QThread>
#include <QMutex>
#include <QByteArray>

class LoopbackRecorderWorker : public QThread {
    Q_OBJECT
public:
    LoopbackRecorderWorker(const QString& filePath, QObject* parent = nullptr);
    void stop();
    QByteArray getAccumulatedPcm();

protected:
    void run() override;

signals:
    void finishedSuccessfully(const QString& filePath);
    void errorOccurred(const QString& error);

private:
    QString m_filePath;
    bool m_running = true;
    QByteArray m_pcmData;
    QMutex m_mutex;
};

class AudioRecorder : public QObject {
    Q_OBJECT
public:
    explicit AudioRecorder(QObject* parent = nullptr);
    ~AudioRecorder();

    void startRecording(const QString& filePath);
    void stopRecording();
    bool isRecording() const { return m_isRecording; }
    QByteArray getAccumulatedPcm();

    static void saveWavFile(const QString& filePath, const QByteArray& pcmData, uint32_t sampleRate = 16000, uint16_t channels = 1);

signals:
    void recordingFinished(const QString& filePath);
    void errorOccurred(const QString& error);

private slots:
    void onWorkerFinished(const QString& filePath);
    void onWorkerError(const QString& error);

private:
    LoopbackRecorderWorker* m_worker = nullptr;
    bool m_isRecording = false;
};

