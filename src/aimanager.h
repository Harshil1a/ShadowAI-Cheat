#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPixmap>

class AIManager : public QObject {
    Q_OBJECT
public:
    explicit AIManager(QObject* parent = nullptr);

    // Send text-only question
    void askText(const QString& userPrompt);
    void loadKeys(); // Reload keys from config

    // Send one or more screenshots + optional text question
    void askWithImages(const QList<QPixmap>& screenshots, const QString& extraPrompt = QString(), const QString& base64Audio = QString(), const QString& audioMimeType = QString());

    // Transcribe audio file first (for OpenAI/Groq) and then send text + images
    void transcribeAudio(const QString& filePath, const QList<QPixmap>& screenshots);

    // Intermediate/Live transcription without solving
    void transcribeAudioOnly(const QString& filePath);
    void cancelTranscriptionOnly();

    bool isBusy() const { return m_busy; }

signals:
    void responseChunk(const QString& text);    // streaming chunks
    void responseComplete(const QString& full); // full final answer
    void errorOccurred(const QString& error);
    void requestStarted();
    void transcriptionOnlyFinished(const QString& text); // live chunk done

private slots:
    void onReplyFinished();
    void onReadyRead();
    void onError(QNetworkReply::NetworkError code);

private:
    void sendRequest(const QJsonDocument& body, bool streaming = true);
    void performRequest(const QList<QPixmap>& screenshots, const QString& extraPrompt, const QString& base64Audio = QString(), const QString& audioMimeType = QString());

    QNetworkAccessManager* m_nam;
    QNetworkReply* m_currentReply = nullptr;
    QNetworkReply* m_transcribeReply = nullptr; // for intermediate transcriptions
    bool m_busy = false;
    bool m_transcribing = false;               // for intermediate transcriptions
    QString m_buffer;           // for streaming SSE buffer
    QString m_fullResponse;

    // --- Key Rotation State ---
    QStringList m_apiKeys;
    int m_currentKeyIndex = 0;
    int m_retryCount = 0;
    QTimer* m_timeoutTimer = nullptr;

    
    // Cache for retrying the last request
    QList<QPixmap> m_lastScreenshots;
    QString m_lastPrompt;
    QString m_lastBase64Audio;
    QString m_lastAudioMime;
};
