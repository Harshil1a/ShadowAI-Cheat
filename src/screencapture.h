#pragma once
#include <QObject>
#include <QPixmap>
#include <QImage>

class ScreenCapture : public QObject {
    Q_OBJECT
public:
    explicit ScreenCapture(QObject* parent = nullptr);

    // Captures entire screen (all monitors) excluding our overlay window
    // overlayHwnd: pass the overlay HWND so we can exclude it
    QPixmap captureFullScreen(void* overlayHwnd = nullptr);

    // Converts QPixmap to base64 JPEG for API
    static QString pixmapToBase64(const QPixmap& pixmap, int quality = 85);

signals:
    void screenshotTaken(const QPixmap& screenshot);

private:
    QPixmap m_lastCapture;
};
