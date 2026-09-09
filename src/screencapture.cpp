#include "screencapture.h"
#include <QScreen>
#include <QApplication>
#include <QBuffer>
#include <QDebug>
#include <QPainter>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

ScreenCapture::ScreenCapture(QObject* parent) : QObject(parent) {}

QPixmap ScreenCapture::captureFullScreen(void* overlayHwnd) {
    Q_UNUSED(overlayHwnd)

    // Use Qt's screen capture for all screens
    QList<QScreen*> screens = QApplication::screens();
    if (screens.isEmpty()) return QPixmap();

    QPixmap result;
    if (screens.size() == 1) {
        result = screens[0]->grabWindow(0);
    } else {
        // Multiple screens: scale each first to save memory
        int targetW = 1920; // Max horizontal resolution for the combined capture
        int totalWidth = 0, maxHeight = 0;
        QList<QPixmap> shots;
        
        for (QScreen* screen : screens) {
            QPixmap screenShot = screen->grabWindow(0);
            // Scale down individual screen if it's too big
            if (screenShot.width() > 1280) {
                screenShot = screenShot.scaledToWidth(1280, Qt::SmoothTransformation);
            }
            shots.append(screenShot);
            totalWidth += screenShot.width();
            maxHeight = qMax(maxHeight, screenShot.height());
        }

        result = QPixmap(totalWidth, maxHeight);
        result.fill(Qt::black);
        QPainter painter(&result);
        int x = 0;
        for (const QPixmap& shot : shots) {
            painter.drawPixmap(x, 0, shot);
            x += shot.width();
        }
        painter.end();
    }

    m_lastCapture = result;
    emit screenshotTaken(result);
    return result;
}

QString ScreenCapture::pixmapToBase64(const QPixmap& pixmap, int quality) {
    if (pixmap.isNull()) return QString();

    QByteArray ba;
    QBuffer buffer(&ba);
    buffer.open(QIODevice::WriteOnly);
    pixmap.save(&buffer, "JPEG", quality);
    buffer.close();

    return QString::fromLatin1(ba.toBase64());
}
