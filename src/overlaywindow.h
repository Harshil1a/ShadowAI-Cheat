#pragma once
#include <QWidget>
#include <QLabel>
#include <QTextBrowser>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPixmap>
#include <QTimer>
#include <QPushButton>
#include <QElapsedTimer>

class AIManager;
class ScreenCapture;
class AudioRecorder;

// The main floating overlay window
class OverlayWindow : public QWidget {
    Q_OBJECT
public:
    explicit OverlayWindow(QWidget* parent = nullptr);
    ~OverlayWindow();

    void setAIManager(AIManager* ai);
    void setScreenCapture(ScreenCapture* sc);

    // Called by HotkeyManager signals
    void toggleVisibility();
    void doScreenshot();
    void doGetAnswer();
    void scrollContentUp();
    void scrollContentDown();
    void cycleTransparency();
    void clearAll();
    void stopAll();
    void refreshSettings();
    void refreshKeyBadges();
    void moveLeft();
    void moveRight();
    void moveUp();
    void moveDown();
    void toggleVoiceRecord();
    void stopVoiceRecordOnRelease();
    void toggleBadgesVisibility();
    void copyScreenshotToClipboard();
    void doGhostWriter();

    void* nativeHandle(); // returns HWND
    void  setCaptureProtection(bool enable);

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

private slots:
    void onAIChunk(const QString& chunk);
    void onAIComplete(const QString& full);
    void onAIError(const QString& err);
    void onAIStarted();
    void animateLoading();
    void onRecordingFinished(const QString& filePath);
    void onTranscribeTimerTimeout();
    void onTranscriptionOnlyFinished(const QString& text);

private:
    void setupUI();
    void applyGlassStyle();
    void updateScreenshotThumb(const QPixmap& px);
    void showStatusMessage(const QString& msg, bool isError = false);
    void setWindowFlags_();
    static QString markdownToHtml(const QString& text);
    static QString stripThinkTags(const QString& text);
    static QString extractCodeBlock(const QString& fullText, bool smartIndent);

    // UI elements
    QWidget*      m_container;
    QWidget*      m_thumbGallery; // New: Container for multiple thumbs
    QTextBrowser* m_answerDisplay;
    QLabel*       m_statusLabel;
    QWidget*      m_screenshotFrame;
    QWidget*      m_controlsPanel;
    QWidget*      m_helpGroupsContainer;
    QLabel*       m_bottomHintLabel;
    QFrame*       m_divider;

    // State
    bool     m_visible      = false;
    double   m_opacity      = 0.88;
    int      m_opacityStep  = 0;
    QList<QPixmap> m_screenshots;
    QString m_accumulatedResponse;
    bool     m_isLoading    = false;
    int      m_loadingDots  = 0;
    QTimer*  m_loadingTimer;
    bool     m_isRecordingAudio = false;
    QElapsedTimer m_recordTimer;
    QTimer*  m_transcribeTimer = nullptr; // added for live transcription

    AIManager*    m_ai    = nullptr;
    ScreenCapture* m_sc   = nullptr;
    AudioRecorder* m_audioRecorder = nullptr;
    QHBoxLayout* m_keysLayout1 = nullptr;
    QHBoxLayout* m_keysLayout2 = nullptr;
    class GhostWriterWorker* m_ghostWriterWorker = nullptr;

    // Opacity presets cycling
    static constexpr double OPACITY_STEPS[] = {1.0, 0.90, 0.80, 0.65, 0.50, 0.40, 0.30, 0.20, 0.15};
    static constexpr int    OPACITY_COUNT   = 9;
};
