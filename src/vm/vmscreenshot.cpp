#include "vmscreenshot.h"
#include <QMouseEvent>
#include <QPainter>
#include <QGuiApplication>
#include <QScreen>
#include <QDebug>

// ── RegionSelector Implementation ──────────────────────────────────────────
RegionSelector::RegionSelector(QWidget* parent) : QWidget(parent) {
    // Fill the parent widget dimensions completely
    if (parent) {
        setGeometry(0, 0, parent->width(), parent->height());
    }
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);
    
    // Ensure this overlay widget doesn't block focus
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
}

RegionSelector::~RegionSelector() {
    if (m_rubberBand) {
        m_rubberBand->deleteLater();
    }
}

void RegionSelector::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_origin = event->pos();
        if (!m_rubberBand) {
            m_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
            
            // Set selection box colors
            QPalette pal;
            pal.setBrush(QPalette::Highlight, QBrush(QColor(0, 229, 255, 120)));
            m_rubberBand->setPalette(pal);
        }
        m_rubberBand->setGeometry(QRect(m_origin, QSize()));
        m_rubberBand->show();
    }
}

void RegionSelector::mouseMoveEvent(QMouseEvent* event) {
    if (m_rubberBand) {
        m_rubberBand->setGeometry(QRect(m_origin, event->pos()).normalized());
    }
}

void RegionSelector::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && m_rubberBand) {
        m_selectedRect = QRect(m_origin, event->pos()).normalized();
        m_rubberBand->hide();
        emit regionSelected(m_selectedRect);
    }
}

void RegionSelector::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    // Draw a semi-transparent screen mask overlay
    painter.fillRect(rect(), QColor(0, 0, 0, 100));
}


// ── VMScreenshotEngine Implementation ──────────────────────────────────────
VMScreenshotEngine::VMScreenshotEngine(QObject* parent) : QObject(parent) {}

VMScreenshotEngine::~VMScreenshotEngine() {
    if (m_selector) {
        m_selector->deleteLater();
    }
}

QPixmap VMScreenshotEngine::captureFull(HWND targetHwnd) {
#ifdef Q_OS_WIN
    if (targetHwnd && IsWindow(targetHwnd)) {
        RECT rect;
        if (GetWindowRect(targetHwnd, &rect)) {
            QScreen* screen = QGuiApplication::primaryScreen();
            if (screen) {
                emit logMessage("Capturing full VM Window area...");
                return screen->grabWindow(0, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
            }
        }
    }
#else
    Q_UNUSED(targetHwnd)
#endif

    // Fallback to full primary screen
    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen) {
        emit logMessage("Capturing primary host screen...");
        return screen->grabWindow(0);
    }
    return QPixmap();
}

QPixmap VMScreenshotEngine::captureActiveWindow(HWND targetHwnd) {
    // For Sandbox, the active window is the VM Window itself
    return captureFull(targetHwnd);
}

void VMScreenshotEngine::startRegionCapture(QWidget* parentContainer, HWND targetHwnd) {
    if (!parentContainer) return;
    
    emit logMessage("Initiating custom region selection overlay...");
    m_currentParent = parentContainer;
    m_targetHwnd = targetHwnd;

    if (m_selector) {
        m_selector->deleteLater();
    }

    m_selector = new RegionSelector(parentContainer);
    connect(m_selector, &RegionSelector::regionSelected, this, &VMScreenshotEngine::onRegionSelected);
    m_selector->show();
    m_selector->raise();
}

void VMScreenshotEngine::onRegionSelected(const QRect& rect) {
    if (m_selector) {
        m_selector->hide();
        m_selector->deleteLater();
        m_selector = nullptr;
    }

    if (!rect.isValid() || rect.width() < 10 || rect.height() < 10) {
        emit logMessage("[WARNING] Selected region was too small. Capture cancelled.");
        return;
    }

    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen || !m_currentParent) return;

    // Convert local widget coordinates to global screen coordinates
    QPoint globalPos = m_currentParent->mapToGlobal(rect.topLeft());
    
    emit logMessage(QString("Region captured: Size %1x%2").arg(rect.width()).arg(rect.height()));
    QPixmap px = screen->grabWindow(0, globalPos.x(), globalPos.y(), rect.width(), rect.height());
    emit screenshotCaptured(px);
}
