#pragma once
#include <QObject>
#include <QPixmap>
#include <QWidget>
#include <QRubberBand>
#include <QPoint>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class RegionSelector : public QWidget {
    Q_OBJECT
public:
    explicit RegionSelector(QWidget* parent = nullptr);
    ~RegionSelector();

signals:
    void regionSelected(const QRect& rect);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    QPoint      m_origin;
    QRubberBand* m_rubberBand = nullptr;
    QRect       m_selectedRect;
};

class VMScreenshotEngine : public QObject {
    Q_OBJECT
public:
    explicit VMScreenshotEngine(QObject* parent = nullptr);
    ~VMScreenshotEngine();

    QPixmap captureFull(HWND targetHwnd);
    QPixmap captureActiveWindow(HWND targetHwnd);
    void startRegionCapture(QWidget* parentContainer, HWND targetHwnd);

signals:
    void screenshotCaptured(const QPixmap& pixmap);
    void logMessage(const QString& msg);

private slots:
    void onRegionSelected(const QRect& rect);

private:
    QWidget* m_currentParent = nullptr;
    HWND     m_targetHwnd = nullptr;
    RegionSelector* m_selector = nullptr;
};
