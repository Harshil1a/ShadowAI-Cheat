#include "overlaywindow.h"
#include "aimanager.h"
#include "screencapture.h"
#include "appconfig.h"
#include "audiorecorder.h"
#include "accountmanager.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QGraphicsDropShadowEffect>
#include <QApplication>
#include <QClipboard>
#include <QScreen>
#include <QFile>
#include <QDir>
#include <QUuid>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif
#include <QScrollBar>
#include <QScroller>
#include <QFrame>
#include <QRegularExpression>
#include <QThread>
#include <atomic>
#include <cstdlib>
#include <ctime>

#ifdef Q_OS_WIN
#ifndef WDA_NONE
#define WDA_NONE 0x00000000
#endif
#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif
#endif

// ─── Ghost Writer Worker Thread ──────────────────────────────────────────────
class GhostWriterWorker : public QThread {
public:
    GhostWriterWorker(const QString& text, int minDelay, int maxDelay, QObject* parent = nullptr)
        : QThread(parent), m_text(text), m_minDelay(minDelay), m_maxDelay(maxDelay), m_stop(false) {}

    ~GhostWriterWorker() {
        requestStop();
        wait();
    }

    void requestStop() { m_stop = true; }

protected:
    void run() override {
        // Wait 400ms for target window focus to settle after overlay hides
        QThread::msleep(400);

        // Seed random number generator locally
        srand(static_cast<uint>(time(nullptr)));

        for (int i = 0; i < m_text.length(); ++i) {
            if (m_stop) break;

            QChar qc = m_text[i];
            ushort ch = qc.unicode();

#ifdef Q_OS_WIN
            if (qc == '\n') {
                // Send simulated Enter Keypress
                INPUT inputs[2] = {};
                inputs[0].type = INPUT_KEYBOARD;
                inputs[0].ki.wVk = VK_RETURN;
                inputs[0].ki.wScan = MapVirtualKey(VK_RETURN, MAPVK_VK_TO_VSC);
                inputs[0].ki.dwFlags = 0; // Key down

                inputs[1].type = INPUT_KEYBOARD;
                inputs[1].ki.wVk = VK_RETURN;
                inputs[1].ki.wScan = inputs[0].ki.wScan;
                inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

                SendInput(2, inputs, sizeof(INPUT));

                // Wait 60ms for the editor's auto-indentation to trigger
                QThread::msleep(60);

                // Type a dummy space to guarantee the line is never empty (prevents Backspace from deleting the newline)
                INPUT spacePress[2] = {};
                spacePress[0].type = INPUT_KEYBOARD;
                spacePress[0].ki.wVk = VK_SPACE;
                spacePress[0].ki.wScan = MapVirtualKey(VK_SPACE, MAPVK_VK_TO_VSC);
                spacePress[0].ki.dwFlags = 0;
                spacePress[1].type = INPUT_KEYBOARD;
                spacePress[1].ki.wVk = VK_SPACE;
                spacePress[1].ki.wScan = spacePress[0].ki.wScan;
                spacePress[1].ki.dwFlags = KEYEVENTF_KEYUP;
                SendInput(2, spacePress, sizeof(INPUT));

                QThread::msleep(15);

                // Send Shift+Home (press Home twice to guarantee selecting to absolute start of line in Monaco/VS Code)
                // 1. Shift down
                INPUT shiftDown = {};
                shiftDown.type = INPUT_KEYBOARD;
                shiftDown.ki.wVk = VK_SHIFT;
                shiftDown.ki.wScan = MapVirtualKey(VK_SHIFT, MAPVK_VK_TO_VSC);
                shiftDown.ki.dwFlags = 0;
                SendInput(1, &shiftDown, sizeof(INPUT));

                // 2. First Home press
                INPUT homePress[2] = {};
                homePress[0].type = INPUT_KEYBOARD;
                homePress[0].ki.wVk = VK_HOME;
                homePress[0].ki.wScan = MapVirtualKey(VK_HOME, MAPVK_VK_TO_VSC);
                homePress[0].ki.dwFlags = KEYEVENTF_EXTENDEDKEY; // Extended key flag
                homePress[1].type = INPUT_KEYBOARD;
                homePress[1].ki.wVk = VK_HOME;
                homePress[1].ki.wScan = homePress[0].ki.wScan;
                homePress[1].ki.dwFlags = KEYEVENTF_KEYUP | KEYEVENTF_EXTENDEDKEY;
                SendInput(2, homePress, sizeof(INPUT));

                QThread::msleep(5);

                // 3. Second Home press (forces cursor to column 0 if first stopped at non-whitespace)
                SendInput(2, homePress, sizeof(INPUT));

                QThread::msleep(15);

                // 4. Shift up
                INPUT shiftUp = {};
                shiftUp.type = INPUT_KEYBOARD;
                shiftUp.ki.wVk = VK_SHIFT;
                shiftUp.ki.wScan = MapVirtualKey(VK_SHIFT, MAPVK_VK_TO_VSC);
                shiftUp.ki.dwFlags = KEYEVENTF_KEYUP;
                SendInput(1, &shiftUp, sizeof(INPUT));

                QThread::msleep(15);

                // 5. Backspace down/up to clear the selection
                INPUT backPress[2] = {};
                backPress[0].type = INPUT_KEYBOARD;
                backPress[0].ki.wVk = VK_BACK;
                backPress[0].ki.wScan = MapVirtualKey(VK_BACK, MAPVK_VK_TO_VSC);
                backPress[0].ki.dwFlags = 0;
                backPress[1].type = INPUT_KEYBOARD;
                backPress[1].ki.wVk = VK_BACK;
                backPress[1].ki.wScan = backPress[0].ki.wScan;
                backPress[1].ki.dwFlags = KEYEVENTF_KEYUP;
                SendInput(2, backPress, sizeof(INPUT));

                // Wait 30ms for the deletion to settle before typing next characters
                QThread::msleep(30);
            } else if (qc == '\t') {
                // Send simulated Tab Keypress
                INPUT inputs[2] = {};
                inputs[0].type = INPUT_KEYBOARD;
                inputs[0].ki.wVk = VK_TAB;
                inputs[0].ki.wScan = MapVirtualKey(VK_TAB, MAPVK_VK_TO_VSC);
                inputs[0].ki.dwFlags = 0; // Key down

                inputs[1].type = INPUT_KEYBOARD;
                inputs[1].ki.wVk = VK_TAB;
                inputs[1].ki.wScan = inputs[0].ki.wScan;
                inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

                SendInput(2, inputs, sizeof(INPUT));
            } else {
                // Send standard Unicode input
                INPUT inputs[2] = {};
                inputs[0].type = INPUT_KEYBOARD;
                inputs[0].ki.wScan = ch;
                inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;

                inputs[1].type = INPUT_KEYBOARD;
                inputs[1].ki.wScan = ch;
                inputs[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;

                SendInput(2, inputs, sizeof(INPUT));
            }

            // Check if this is an opening bracket/quote that editors auto-close
            bool isAutoCloseChar = (qc == '{' || qc == '(' || qc == '[' || qc == '"' || qc == '\'');
            if (isAutoCloseChar) {
                // Wait 30ms for the editor to auto-insert the closing character
                QThread::msleep(30);

                // Send simulated Delete keypress to wipe out the auto-inserted character
                INPUT deletePress[2] = {};
                deletePress[0].type = INPUT_KEYBOARD;
                deletePress[0].ki.wVk = VK_DELETE;
                deletePress[0].ki.wScan = MapVirtualKey(VK_DELETE, MAPVK_VK_TO_VSC);
                deletePress[0].ki.dwFlags = KEYEVENTF_EXTENDEDKEY; // Extended key flag

                deletePress[1].type = INPUT_KEYBOARD;
                deletePress[1].ki.wVk = VK_DELETE;
                deletePress[1].ki.wScan = deletePress[0].ki.wScan;
                deletePress[1].ki.dwFlags = KEYEVENTF_KEYUP | KEYEVENTF_EXTENDEDKEY;

                SendInput(2, deletePress, sizeof(INPUT));

                // Wait 15ms for deletion to complete before typing next characters
                QThread::msleep(15);
            }
#else
            // Fallback for non-Windows platforms
            Q_UNUSED(ch);
            QThread::msleep(15);
#endif

            // Generate a natural humanized delay within bounds
            int range = qMax(1, m_maxDelay - m_minDelay);
            int delay = m_minDelay + (rand() % range);

            // Contextual human pacing
            if (qc == '\n') {
                delay += 80 + (rand() % 40); // extra wait on newline
            } else if (qc == ' ' || qc == '\t') {
                delay += 10 + (rand() % 15); // extra wait on whitespace
            } else if (qc == ';' || qc == '.' || qc == ',') {
                delay += 30 + (rand() % 30); // extra wait on boundaries
            }

            QThread::msleep(delay);
        }
    }

private:
    QString m_text;
    int m_minDelay;
    int m_maxDelay;
    std::atomic<bool> m_stop;
};

constexpr double OverlayWindow::OPACITY_STEPS[];

OverlayWindow::OverlayWindow(QWidget* parent)
    : QWidget(parent)
    , m_loadingTimer(new QTimer(this))
{
    setWindowFlags_();
    setupUI();
    applyGlassStyle();
    
    m_audioRecorder = new AudioRecorder(this);
    connect(m_audioRecorder, &AudioRecorder::recordingFinished, this, &OverlayWindow::onRecordingFinished);
    connect(m_audioRecorder, &AudioRecorder::errorOccurred, this, [this](const QString& err) {
        showStatusMessage("Audio Error: " + err, true);
    });

    // Make all child widgets transparent to mouse events
    auto children = findChildren<QWidget*>();
    for (auto child : children) {
        child->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }

    // Restore saved position
    auto& cfg = AppConfig::instance();
    move(cfg.overlayX(), cfg.overlayY());
    m_opacity = cfg.overlayOpacity();
    setWindowOpacity(m_opacity);

    // Loading animation timer
    connect(m_loadingTimer, &QTimer::timeout, this, &OverlayWindow::animateLoading);

    // Enable screen capture protection by default
    setCaptureProtection(true);

    // Start hidden
    hide();
}

OverlayWindow::~OverlayWindow() {
    if (m_ghostWriterWorker) {
        m_ghostWriterWorker->requestStop();
        m_ghostWriterWorker->wait();
    }
    auto& cfg = AppConfig::instance();
    cfg.setOverlayPos(x(), y());
    cfg.setOverlayOpacity(m_opacity);
    cfg.save();
}

void OverlayWindow::setWindowFlags_() {
    // Tool window: no taskbar entry, no focus steal, always on top, frameless
    setWindowFlags(
        Qt::FramelessWindowHint       |
        Qt::WindowStaysOnTopHint      |
        Qt::Tool                      |   // No taskbar button
        Qt::WindowDoesNotAcceptFocus  |   // Prevent focus completely
        Qt::BypassWindowManagerHint   |
        Qt::WindowTransparentForInput     // KEY: Qt-level click through
    );
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);  // KEY: no focus steal
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);  // Mouse passes through

#ifdef Q_OS_WIN
    // Additional Windows-specific: make click-through for non-content areas
    // and ensure it doesn't appear in Alt+Tab
    // We'll set these after window is created in showEvent
#endif
}

void OverlayWindow::setupUI() {
    auto& cfg = AppConfig::instance();
    int w = cfg.overlayWidth();
    int h = cfg.overlayHeight();
    
    setFixedSize(w, h);

    // Main container
    m_container = new QWidget(this);
    m_container->setGeometry(0, 0, w, h);
    m_container->setObjectName("container");

    QVBoxLayout* mainLayout = new QVBoxLayout(m_container);
    mainLayout->setContentsMargins(18, 18, 18, 14);
    mainLayout->setSpacing(6);

    // ── TOP BAR ──────────────────────────────────────────────────────────
    QWidget* topBar = new QWidget;
    topBar->setObjectName("topBar");
    topBar->setFixedHeight(32);
    QHBoxLayout* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(8, 0, 8, 0);
    topLayout->setSpacing(8);

    QLabel* appIcon = new QLabel(QString::fromUtf8("◈"));
    appIcon->setObjectName("appIcon");

    QLabel* appName = new QLabel("System Broker");
    appName->setObjectName("appName");

    // No version badge for stealth

    m_statusLabel = new QLabel("Ready");
    m_statusLabel->setObjectName("statusLabel");
    m_statusLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_creditsBadge = new QLabel(topBar);
    m_creditsBadge->setObjectName("creditsBadge");
    m_creditsBadge->setAlignment(Qt::AlignCenter);

    topLayout->addWidget(appIcon);
    topLayout->addWidget(appName);
    topLayout->addStretch();
    topLayout->addWidget(m_creditsBadge);
    topLayout->addWidget(m_statusLabel);

    connect(&AccountManager::instance(), &AccountManager::creditsUpdated, this, [this](int) {
        updateCreditsBadge();
    });
    connect(&AccountManager::instance(), &AccountManager::accountStateChanged, this, [this](bool, const QString&, bool) {
        updateCreditsBadge();
    });
    updateCreditsBadge();

    // ── SCREENSHOT PANEL ─────────────────────────────────────────────────
    m_screenshotFrame = new QWidget;
    m_screenshotFrame->setObjectName("screenshotFrame");
    m_screenshotFrame->setFixedHeight(105);
    QVBoxLayout* frameLayout = new QVBoxLayout(m_screenshotFrame);
    frameLayout->setContentsMargins(8, 6, 8, 6);
    frameLayout->setSpacing(2);

    QScrollArea* scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setObjectName("thumbScrollArea");

    m_thumbGallery = new QWidget;
    m_thumbGallery->setObjectName("thumbGallery");
    QHBoxLayout* galleryLayout = new QHBoxLayout(m_thumbGallery);
    galleryLayout->setContentsMargins(0, 0, 0, 0);
    galleryLayout->setSpacing(8);
    galleryLayout->setAlignment(Qt::AlignLeft);
    
    QLabel* placeholder = new QLabel(QString::fromUtf8("📷  No screenshots yet — press Shift+Alt+S to capture"));
    placeholder->setObjectName("screenshotThumb");
    placeholder->setAlignment(Qt::AlignCenter);
    galleryLayout->addWidget(placeholder);

    scrollArea->setWidget(m_thumbGallery);
    frameLayout->addWidget(scrollArea);

    // ── DIVIDER ──────────────────────────────────────────────────────────
    m_divider = new QFrame;
    m_divider->setFrameShape(QFrame::HLine);
    m_divider->setObjectName("divider");

    // ── ANSWER DISPLAY & ALL KEYS HUD STACK ───────────────────────────────
    m_contentStack = new QStackedWidget;
    m_contentStack->setObjectName("contentStack");

    m_answerDisplay = new QTextBrowser;
    m_answerDisplay->setObjectName("answerDisplay");
    m_answerDisplay->setOpenExternalLinks(false);
    m_answerDisplay->setReadOnly(true);
    m_answerDisplay->installEventFilter(this);
    QScroller::grabGesture(m_answerDisplay->viewport(), QScroller::TouchGesture);

    m_allKeysHUD = new QWidget;
    m_allKeysHUD->setObjectName("allKeysHUD");
    m_allKeysLayout = new QVBoxLayout(m_allKeysHUD);
    m_allKeysLayout->setContentsMargins(6, 6, 6, 6);
    m_allKeysLayout->setSpacing(4);

    m_contentStack->addWidget(m_answerDisplay); // Index 0
    m_contentStack->addWidget(m_allKeysHUD);     // Index 1
    m_contentStack->setCurrentIndex(0);

    // ── HOTKEY CONTROLS ──────────────────────────────────────────────────
    m_controlsPanel = new QWidget;
    m_controlsPanel->setObjectName("controlsPanel");
    QVBoxLayout* cpLayout = new QVBoxLayout(m_controlsPanel);
    cpLayout->setContentsMargins(0, 4, 0, 0);
    cpLayout->setSpacing(5);

    auto makeGroup = [](const QString& title) -> QFrame* {
        QFrame* f = new QFrame;
        f->setProperty("class", "controlGroup");
        QVBoxLayout* l = new QVBoxLayout(f);
        l->setContentsMargins(10, 6, 10, 8);
        l->setSpacing(6);
        
        QLabel* t = new QLabel(title);
        t->setProperty("class", "groupTitle");
        t->setAlignment(Qt::AlignCenter);
        l->addWidget(t);
        
        QHBoxLayout* hl = new QHBoxLayout;
        hl->setObjectName("keysLayout");
        hl->setSpacing(6);
        l->addLayout(hl);
        return f;
    };

    auto makeKey = [](const QString& key, const QString& action) -> QWidget* {
        QWidget* w = new QWidget;
        QVBoxLayout* l = new QVBoxLayout(w);
        l->setContentsMargins(0, 0, 0, 0);
        l->setSpacing(3);
        l->setAlignment(Qt::AlignCenter);
        
        QLabel* kLbl = new QLabel(key);
        kLbl->setProperty("class", "keyBadge");
        kLbl->setAlignment(Qt::AlignCenter);
        
        QLabel* aLbl = new QLabel(action);
        aLbl->setProperty("class", "keyText");
        aLbl->setAlignment(Qt::AlignCenter);
        
        l->addWidget(kLbl);
        l->addWidget(aLbl);
        return w;
    };

    QFrame* group1 = makeGroup("Quick Controls");
    m_keysLayout1 = group1->findChild<QHBoxLayout*>("keysLayout");
    m_keysLayout2 = nullptr;

    // Container for helper groups so they can be hidden together
    m_helpGroupsContainer = new QWidget;
    m_helpGroupsContainer->setObjectName("helpGroupsContainer");
    QVBoxLayout* hgLayout = new QVBoxLayout(m_helpGroupsContainer);
    hgLayout->setContentsMargins(0, 0, 0, 0);
    hgLayout->setSpacing(0);
    hgLayout->addWidget(group1);

    // Label that remains visible when badges are hidden
    m_bottomHintLabel = new QLabel("[Shift+Alt+B] All Keys Directory");
    m_bottomHintLabel->setObjectName("bottomHintLabel");
    m_bottomHintLabel->setAlignment(Qt::AlignCenter);
    m_bottomHintLabel->setVisible(false); // Hidden by default

    refreshKeyBadges();

    cpLayout->addWidget(m_helpGroupsContainer);
    cpLayout->addWidget(m_bottomHintLabel);

    mainLayout->addWidget(topBar);
    mainLayout->addWidget(m_screenshotFrame);
    mainLayout->addWidget(m_divider);
    mainLayout->addWidget(m_contentStack, 1);
    mainLayout->addWidget(m_controlsPanel);
}

void OverlayWindow::applyGlassStyle() {
    setStyleSheet(R"(
        QWidget#container {
            background: transparent;
        }

        /* ═══ TOP BAR — semi-transparent raised header ═══ */
        QWidget#topBar {
            background: rgba(18, 18, 26, 180);
            border: 1px solid rgba(255, 255, 255, 0.15);
            border-top: 1px solid rgba(255, 255, 255, 0.30);
            border-radius: 8px;
        }

        QLabel#appIcon {
            color: #ffffff;
            font-size: 16px;
            font-weight: bold;
            background: transparent;
        }

        QLabel#appName {
            color: #ffffff;
            font-family: "Segoe UI", -apple-system, system-ui, sans-serif;
            font-size: 13px;
            font-weight: bold;
            letter-spacing: 2px;
            background: transparent;
        }

        QLabel#statusLabel {
            color: rgba(255, 255, 255, 0.90);
            font-family: "Consolas", monospace;
            font-size: 11px;
            font-weight: bold;
            background: transparent;
        }

        /* ═══ SCREENSHOT FRAME — semi-transparent inset panel ═══ */
        QWidget#screenshotFrame {
            background: rgba(18, 18, 26, 160);
            border-top: 1px solid rgba(0, 0, 0, 0.6);
            border-left: 1px solid rgba(0, 0, 0, 0.55);
            border-right: 1px solid rgba(255, 255, 255, 0.10);
            border-bottom: 1px solid rgba(255, 255, 255, 0.14);
            border-radius: 8px;
        }

        QScrollArea#thumbScrollArea {
            background: transparent;
            border: none;
        }

        QLabel#screenshotThumb {
            color: rgba(255, 255, 255, 0.65);
            font-family: "Segoe UI", sans-serif;
            font-size: 11px;
            background: transparent;
        }

        /* ═══ DIVIDER — 3D groove ═══ */
        QFrame#divider {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 transparent,
                stop:0.2 rgba(0, 0, 0, 0.3),
                stop:0.5 rgba(255, 255, 255, 0.15),
                stop:0.8 rgba(0, 0, 0, 0.3),
                stop:1 transparent);
            max-height: 1px;
            border: none;
        }

        /* ═══ ANSWER DISPLAY — semi-transparent embossed panel ═══ */
        QTextBrowser#answerDisplay {
            background: rgba(14, 14, 20, 170);
            border-top: 1px solid rgba(0, 0, 0, 0.6);
            border-left: 1px solid rgba(0, 0, 0, 0.55);
            border-right: 1px solid rgba(255, 255, 255, 0.10);
            border-bottom: 1px solid rgba(255, 255, 255, 0.14);
            border-radius: 8px;
            color: #ffffff;
            font-family: "Segoe UI", "Arial", sans-serif;
            font-size: 13px;
            line-height: 1.7;
            padding: 12px 14px;
            selection-background-color: rgba(100, 160, 255, 0.30);
        }

        QTextBrowser#answerDisplay QScrollBar:vertical {
            background: rgba(255, 255, 255, 0.06);
            width: 6px;
            border-radius: 3px;
        }
        QTextBrowser#answerDisplay QScrollBar::handle:vertical {
            background: rgba(255, 255, 255, 0.25);
            border-radius: 3px;
            min-height: 24px;
        }
        QTextBrowser#answerDisplay QScrollBar::add-line:vertical,
        QTextBrowser#answerDisplay QScrollBar::sub-line:vertical {
            height: 0px;
        }

        /* ═══ HOTKEY PANELS — semi-transparent raised cards ═══ */
        .controlGroup {
            background: rgba(18, 18, 26, 160);
            border-top: 1px solid rgba(255, 255, 255, 0.14);
            border-left: 1px solid rgba(255, 255, 255, 0.10);
            border-right: 1px solid rgba(0, 0, 0, 0.45);
            border-bottom: 1px solid rgba(0, 0, 0, 0.55);
            border-radius: 8px;
        }
        
        .groupTitle {
            color: #8b9cc0;
            font-family: "Segoe UI", sans-serif;
            font-size: 10px;
            font-weight: 700;
            letter-spacing: 1px;
        }
        
        /* ═══ KEY BADGES — semi-transparent keycap look ═══ */
        .keyBadge {
            background: rgba(10, 10, 14, 180);
            color: #ffffff;
            border-top: 1px solid rgba(255, 255, 255, 0.28);
            border-left: 1px solid rgba(255, 255, 255, 0.20);
            border-right: 1px solid rgba(0, 0, 0, 0.35);
            border-bottom: 2px solid rgba(0, 0, 0, 0.50);
            border-radius: 5px;
            padding: 3px 8px;
            font-family: "Consolas", monospace;
            font-size: 10px;
            font-weight: bold;
        }
        
        .keyText {
            color: #b0bdd4;
            font-family: "Segoe UI", sans-serif;
            font-size: 9px;
            font-weight: 600;
        }

        /* ═══ ALL KEYS DIRECTORY HUD ═══ */
        QWidget#allKeysHUD {
            background: rgba(14, 14, 20, 220);
            border: 1px solid rgba(0, 229, 255, 0.25);
            border-radius: 8px;
        }

        QLabel#hudHeader {
            color: #00e5ff;
            font-family: "Segoe UI", -apple-system, sans-serif;
            font-size: 11px;
            font-weight: 800;
            letter-spacing: 1.5px;
            padding: 2px 0;
            background: transparent;
        }

        QScrollArea#hudScrollArea {
            background: transparent;
            border: none;
        }

        .hudCategoryCard {
            background: rgba(18, 22, 32, 200);
            border: 1px solid rgba(0, 229, 255, 0.18);
            border-radius: 6px;
        }

        .hudCategoryTitle {
            color: #00e5ff;
            font-family: "Segoe UI", sans-serif;
            font-size: 10px;
            font-weight: 800;
            letter-spacing: 1px;
            margin-bottom: 2px;
        }

        .hudBadge {
            background: rgba(0, 229, 255, 0.14);
            color: #ffffff;
            border: 1px solid rgba(0, 229, 255, 0.40);
            border-radius: 4px;
            padding: 2px 6px;
            font-family: "Consolas", monospace;
            font-size: 9px;
            font-weight: bold;
        }

        .hudBadgeDanger {
            background: rgba(255, 60, 60, 0.22);
            color: #ff6b6b;
            border: 1px solid #ff4444;
            border-radius: 4px;
            padding: 2px 6px;
            font-family: "Consolas", monospace;
            font-size: 9px;
            font-weight: bold;
        }

        .hudAction {
            color: #d0e4f5;
            font-family: "Segoe UI", sans-serif;
            font-size: 10px;
            font-weight: 500;
        }

        .hudActionDanger {
            color: #ff8b8b;
            font-family: "Segoe UI", sans-serif;
            font-size: 10px;
            font-weight: bold;
        }

        QLabel#hudFooter {
            color: #00e5ff;
            font-family: "Consolas", monospace;
            font-size: 10px;
            font-weight: bold;
            padding: 2px 0;
            background: transparent;
        }

        /* ═══ BOTTOM HELP HINT ═══ */
        QLabel#bottomHintLabel {
            color: rgba(255, 255, 255, 0.70);
            font-family: "Segoe UI", sans-serif;
            font-size: 11px;
            font-weight: 600;
            padding: 3px 0;
            background: transparent;
        }
    )");
}

void OverlayWindow::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();
    int rad = 12;

    // ── OUTER GLOW — multi-layer white/translucent aura ────────────────────
    for (int i = 10; i >= 1; --i) {
        QPainterPath glowPath;
        glowPath.addRoundedRect(i, i, w - 2*i, h - 2*i, rad + i, rad + i);
        int alpha = (i <= 3) ? 10 : 4;
        QColor glowColor = QColor(255, 255, 255, alpha);
        p.setPen(QPen(glowColor, 1));
        p.setBrush(Qt::NoBrush);
        p.drawPath(glowPath);
    }

    // ── MAIN PANEL ───────────────────────────────────────────────────
    QPainterPath mainPath;
    mainPath.addRoundedRect(3, 3, w - 6, h - 6, rad, rad);

    // Semi-transparent dark fill for see-through effect
    p.fillPath(mainPath, QColor(20, 20, 28, 200));

    // ── 3D EDGE LIGHTING ─────────────────────────────────────────────
    // Top edge — bright (light from above)
    QLinearGradient topEdge(rad + 3, 3, w - rad - 3, 3);
    topEdge.setColorAt(0.0, Qt::transparent);
    topEdge.setColorAt(0.15, QColor(255, 255, 255, 40));
    topEdge.setColorAt(0.5, QColor(255, 255, 255, 100));
    topEdge.setColorAt(0.85, QColor(255, 255, 255, 40));
    topEdge.setColorAt(1.0, Qt::transparent);
    p.setPen(QPen(QBrush(topEdge), 1.5));
    p.drawLine(QPointF(rad + 10, 3.5), QPointF(w - rad - 10, 3.5));

    // Left edge — subtle highlight
    p.setPen(QPen(QColor(255, 255, 255, 15), 1));
    p.drawLine(QPointF(3.5, rad + 10), QPointF(3.5, h - rad - 10));

    // Bottom edge — shadow
    p.setPen(QPen(QColor(0, 0, 0, 80), 1));
    p.drawLine(QPointF(rad + 10, h - 3.5), QPointF(w - rad - 10, h - 3.5));

    // Right edge — shadow
    p.setPen(QPen(QColor(0, 0, 0, 60), 1));
    p.drawLine(QPointF(w - 3.5, rad + 10), QPointF(w - 3.5, h - rad - 10));

    // ── PANEL BORDER ─────────────────────────────────────────────────
    p.setPen(QPen(QColor(255, 255, 255, 25), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(3, 3, w - 6, h - 6), rad, rad);

    // ── INNER TOP HIGHLIGHT BAR (subtle glass reflection) ────────────
    QPainterPath reflectionPath;
    reflectionPath.addRoundedRect(6, 6, w - 12, 40, rad - 2, rad - 2);
    QLinearGradient reflectionGrad(0, 6, 0, 46);
    reflectionGrad.setColorAt(0.0, QColor(255, 255, 255, 8));
    reflectionGrad.setColorAt(1.0, QColor(255, 255, 255, 0));
    p.fillPath(reflectionPath, reflectionGrad);

    // ── CORNER ACCENT DOTS ───────────────────────────────────────────
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 25));
    int cd = 14;
    p.drawEllipse(QPointF(cd, cd), 2, 2);
    p.drawEllipse(QPointF(w - cd, cd), 2, 2);
    p.drawEllipse(QPointF(cd, h - cd), 2, 2);
    p.drawEllipse(QPointF(w - cd, h - cd), 2, 2);
}
void OverlayWindow::setAIManager(AIManager* ai) {
    m_ai = ai;
    connect(ai, &AIManager::responseChunk,    this, &OverlayWindow::onAIChunk);
    connect(ai, &AIManager::responseComplete, this, &OverlayWindow::onAIComplete);
    connect(ai, &AIManager::errorOccurred,    this, &OverlayWindow::onAIError);
    connect(ai, &AIManager::requestStarted,   this, &OverlayWindow::onAIStarted);
    connect(ai, &AIManager::transcriptionOnlyFinished, this, &OverlayWindow::onTranscriptionOnlyFinished);
}

void OverlayWindow::setScreenCapture(ScreenCapture* sc) {
    m_sc = sc;
}

void* OverlayWindow::nativeHandle() {
#ifdef Q_OS_WIN
    return reinterpret_cast<void*>(winId());
#else
    return nullptr;
#endif
}

void OverlayWindow::setCaptureProtection(bool enable) {
#ifdef Q_OS_WIN
    HWND hwnd = static_cast<HWND>(nativeHandle());
    if (hwnd) {
        DWORD affinity = enable ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE;
        if (!SetWindowDisplayAffinity(hwnd, affinity)) {
            // Debug output removed for stealth
        }
    }
#else
    Q_UNUSED(enable)
#endif
}

// ─── Hotkey actions ──────────────────────────────────────────────────────────

void OverlayWindow::toggleVisibility() {
    if (isVisible()) {
        hide();
        m_visible = false;
    } else {
        show();
        raise();
        // DO NOT call activateWindow() - that steals focus!
        m_visible = true;

#ifdef Q_OS_WIN
        // Ensure window is not in Alt+Tab and doesn't steal focus
        HWND hwnd = static_cast<HWND>(nativeHandle());
        LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
        
        // WS_EX_TRANSPARENT + WS_EX_LAYERED = click through
        // WS_EX_NOACTIVATE = don't take focus on click/show
        // WS_EX_TOOLWINDOW = no taskbar, no Alt+Tab
        exStyle |= WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_LAYERED;
        exStyle &= ~WS_EX_APPWINDOW;
        SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);

        // Keep it on top without stealing focus
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
#endif
    }
}

void OverlayWindow::doScreenshot() {
    if (!m_sc) return;
    showStatusMessage("Capturing...");

    QTimer::singleShot(50, this, [this]() {
        QPixmap shot = m_sc->captureFullScreen(nativeHandle());
        if (!shot.isNull()) {
            m_screenshots.append(shot);
            updateScreenshotThumb(shot);
            showStatusMessage(QString("Screenshot added (%1 total) → Shift+Alt+A").arg(m_screenshots.size()));
        }
    });
}

void OverlayWindow::doGetAnswer() {
    if (!m_ai) return;
    
    // Check if AI is already busy to prevent spamming requests (prevents 429)
    if (m_ai->isBusy()) {
        showStatusMessage("Still thinking... please wait", true);
        return;
    }

    if (m_screenshots.isEmpty()) {
        showStatusMessage("No screenshots! Press Shift+Alt+S first", true);
        return;
    }
    showStatusMessage(QString("Sending %1 images to AI...").arg(m_screenshots.size()));
    if (m_contentStack) {
        m_contentStack->setCurrentIndex(0);
    }
    m_answerDisplay->clear();
    m_ai->askWithImages(m_screenshots);
}

void OverlayWindow::scrollContentUp() {
    QScrollBar* sb = m_answerDisplay->verticalScrollBar();
    if (sb) sb->setValue(sb->value() - 60);
}

void OverlayWindow::scrollContentDown() {
    QScrollBar* sb = m_answerDisplay->verticalScrollBar();
    if (sb) sb->setValue(sb->value() + 60);
}

void OverlayWindow::cycleTransparency() {
    m_opacityStep = (m_opacityStep + 1) % OPACITY_COUNT;
    m_opacity = OPACITY_STEPS[m_opacityStep];
    setWindowOpacity(m_opacity);

    int pct = static_cast<int>(m_opacity * 100);
    showStatusMessage(QString("Opacity: %1%").arg(pct));

    auto& cfg = AppConfig::instance();
    cfg.setOverlayOpacity(m_opacity);
    cfg.save();
}

void OverlayWindow::clearAll() {
    if (m_ghostWriterWorker) {
        m_ghostWriterWorker->requestStop();
    }
    if (m_contentStack) {
        m_contentStack->setCurrentIndex(0);
    }
    m_answerDisplay->clear();
    m_screenshots.clear();
    m_accumulatedResponse.clear();
    
    // Clear gallery UI
    QLayout* layout = m_thumbGallery->layout();
    QLayoutItem* item;
    while ((item = layout->takeAt(0)) != nullptr) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    
    QLabel* placeholder = new QLabel("Cleared. Press Shift+Alt+S to capture again.");
    placeholder->setObjectName("screenshotThumb");
    placeholder->setAlignment(Qt::AlignCenter);
    layout->addWidget(placeholder);
    
    showStatusMessage("Cleared");
}

void OverlayWindow::stopAll() {
    m_loadingTimer->stop();
    m_isLoading = false;
    clearAll();
    hide();
}

static QString vkToKeyName(int vk) {
    if (vk >= 0x41 && vk <= 0x5A) return QString(QChar(vk));
    if (vk >= 0x30 && vk <= 0x39) return QString(QChar(vk));
    if (vk >= 0x70 && vk <= 0x7B) return QString("F%1").arg(vk - 0x6F);
    switch (vk) {
        case 0x20: return "Space";
        case 0x0D: return "Enter";
        case 0x1B: return "Esc";
        case 0x08: return "Backspace";
        case 0x09: return "Tab";
        case 0x2E: return "Del";
        case 0x25: return "Left";
        case 0x26: return "Up";
        case 0x27: return "Right";
        case 0x28: return "Down";
        default:   return QString("0x%1").arg(vk, 2, 16, QChar('0')).toUpper();
    }
}

void OverlayWindow::buildAllKeysHUD() {
    if (!m_allKeysHUD || !m_allKeysLayout) return;

    // Clear previous items in m_allKeysLayout
    QLayoutItem* item;
    while ((item = m_allKeysLayout->takeAt(0)) != nullptr) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    auto& cfg = AppConfig::instance();

    QLabel* header = new QLabel(QString::fromUtf8("◈ SHADOW_AI // SHORTCUTS DIRECTORY"));
    header->setObjectName("hudHeader");
    header->setAlignment(Qt::AlignCenter);
    m_allKeysLayout->addWidget(header);

    // ZERO SCROLLING: Symmetrical, compact 2x2 grid fitting 100% in single view
    QGridLayout* gridLayout = new QGridLayout;
    gridLayout->setContentsMargins(4, 2, 4, 2);
    gridLayout->setSpacing(6);

    auto makeCategoryCard = [](const QString& title, const QList<QPair<QString, QString>>& items, bool isEmergency = false) -> QWidget* {
        QFrame* card = new QFrame;
        card->setProperty("class", "hudCategoryCard");
        QVBoxLayout* cl = new QVBoxLayout(card);
        cl->setContentsMargins(8, 6, 8, 6);
        cl->setSpacing(3);

        QLabel* tLbl = new QLabel(title);
        tLbl->setProperty("class", "hudCategoryTitle");
        if (isEmergency) {
            tLbl->setStyleSheet("color: #ff6b6b; font-weight: bold; font-family: 'Segoe UI', sans-serif; font-size: 12px;");
        } else {
            tLbl->setStyleSheet("font-weight: bold; font-family: 'Segoe UI', sans-serif; font-size: 12px; color: #a8edea;");
        }
        cl->addWidget(tLbl);

        for (const auto& pair : items) {
            QWidget* row = new QWidget;
            QHBoxLayout* rl = new QHBoxLayout(row);
            rl->setContentsMargins(0, 1, 0, 1);
            rl->setSpacing(6);

            QLabel* kBadge = new QLabel(pair.first);
            kBadge->setProperty("class", isEmergency ? "hudBadgeDanger" : "hudBadge");
            kBadge->setAlignment(Qt::AlignCenter);
            kBadge->setMinimumWidth(120);
            kBadge->setStyleSheet(kBadge->styleSheet() + "font-size: 11px;");

            QLabel* aLbl = new QLabel(pair.second);
            aLbl->setProperty("class", isEmergency ? "hudActionDanger" : "hudAction");
            aLbl->setStyleSheet("font-size: 11px;");

            rl->addWidget(kBadge);
            rl->addWidget(aLbl, 1);
            cl->addWidget(row);
        }
        return card;
    };

    // 1. AI & Capture (Row 0, Col 0)
    QList<QPair<QString, QString>> aiItems = {
        {"Shift+Alt+" + vkToKeyName(cfg.hotkeyScreenshot()), "Capture Screen Selection"},
        {"Shift+Alt+" + vkToKeyName(cfg.hotkeyGetAnswer()), "Snap & Solve (AI Solution)"},
        {"Shift+Alt+" + vkToKeyName(cfg.hotkeyVoice()), "Microphone Voice Input"},
        {"Shift+Alt+" + vkToKeyName(cfg.hotkeyGhostWriter()), "Auto-Type Ghost Writer"},
        {"Shift+Alt+" + vkToKeyName(cfg.hotkeyCopyScreenshot()), "Copy Last Screen Snapshot"}
    };
    gridLayout->addWidget(makeCategoryCard("📸 AI & CAPTURE CONTROLS", aiItems), 0, 0);

    // 2. View & Stealth (Row 0, Col 1)
    QList<QPair<QString, QString>> viewItems = {
        {"Shift+Alt+" + vkToKeyName(cfg.hotkeyToggle()), "Show / Hide Overlay Window"},
        {"Shift+Alt+" + vkToKeyName(cfg.hotkeyTransparency()), "Cycle Transparency (9 Presets)"},
        {"Shift+Alt+" + vkToKeyName(cfg.hotkeyClear()), "Clear Output & Chat History"},
        {"Shift+Alt+" + vkToKeyName(cfg.hotkeyToggleBadges()), "Toggle All Keys Directory HUD"}
    };
    gridLayout->addWidget(makeCategoryCard("🪟 OVERLAY & STEALTH CONTROLS", viewItems), 0, 1);

    // 3. Navigation & Movement (Row 1, Col 0)
    QList<QPair<QString, QString>> navItems = {
        {"Shift+Alt+" + vkToKeyName(cfg.hotkeyMoveLeft()) + "/" + vkToKeyName(cfg.hotkeyMoveRight()), "Nudge Window Left / Right"},
        {"Shift+Alt+" + vkToKeyName(cfg.hotkeyMoveUp()) + "/" + vkToKeyName(cfg.hotkeyMoveDown()), "Nudge Window Up / Down"},
        {"Shift+Alt+" + vkToKeyName(cfg.hotkeyScrollUp()) + "/" + vkToKeyName(cfg.hotkeyScrollDown()), "Scroll Answer Output Up / Down"}
    };
    gridLayout->addWidget(makeCategoryCard("🧭 NAVIGATION & MOVEMENT", navItems), 1, 0);

    // 4. Emergency (Row 1, Col 1) — no duplicate, just panic info
    QList<QPair<QString, QString>> emergItems = {
        {"Ctrl+Shift+" + vkToKeyName(cfg.hotkeyPanic()), "PANIC KILL (Wipe Process & Clipboard)"},
        {"Shift+Alt+" + vkToKeyName(cfg.hotkeyHideStrip()), "Hide Key Strip (Clean Answer View)"}
    };
    gridLayout->addWidget(makeCategoryCard("🚨 EMERGENCY & CONTROLS", emergItems, true), 1, 1);

    m_allKeysLayout->addLayout(gridLayout, 1);

    QLabel* footer = new QLabel(QString("[Shift+Alt+%1] Press hotkey again to return to AI answer").arg(vkToKeyName(cfg.hotkeyToggleBadges())));
    footer->setObjectName("hudFooter");
    footer->setAlignment(Qt::AlignCenter);
    m_allKeysLayout->addWidget(footer);
}

void OverlayWindow::refreshKeyBadges() {
    if (!m_keysLayout1 || !m_keysLayout2) return;

    auto clearLayout = [](QLayout* layout) {
        if (!layout) return;
        QLayoutItem* item;
        while ((item = layout->takeAt(0)) != nullptr) {
            if (item->widget()) {
                item->widget()->deleteLater();
            }
            delete item;
        }
    };

    clearLayout(m_keysLayout1);
    clearLayout(m_keysLayout2);

    auto makeKey = [](const QString& key, const QString& action, bool isPanic = false) -> QWidget* {
        QWidget* w = new QWidget;
        QVBoxLayout* l = new QVBoxLayout(w);
        l->setContentsMargins(0, 0, 0, 0);
        l->setSpacing(3);
        l->setAlignment(Qt::AlignCenter);
        
        QLabel* kLbl = new QLabel(key);
        kLbl->setProperty("class", "keyBadge");
        kLbl->setAlignment(Qt::AlignCenter);
        if (isPanic) {
            kLbl->setStyleSheet("background: rgba(255, 60, 60, 0.25); border: 1px solid #ff4444; color: #ff6b6b; font-weight: bold; border-radius: 4px; padding: 2px 6px; font-size: 10px; font-family: 'Consolas', monospace;");
        }
        
        QLabel* aLbl = new QLabel(action);
        aLbl->setProperty("class", "keyText");
        aLbl->setAlignment(Qt::AlignCenter);
        if (isPanic) {
            aLbl->setStyleSheet("color: #ff6b6b; font-size: 9px; font-weight: bold;");
        }
        
        l->addWidget(kLbl);
        l->addWidget(aLbl);
        return w;
    };

    auto& cfg = AppConfig::instance();

    m_keysLayout1->addWidget(makeKey("Shift+Alt+" + vkToKeyName(cfg.hotkeyGetAnswer()), "Snap & Solve"));
    m_keysLayout1->addWidget(makeKey("Shift+Alt+" + vkToKeyName(cfg.hotkeyScreenshot()), "Screenshot"));
    m_keysLayout1->addWidget(makeKey("Shift+Alt+" + vkToKeyName(cfg.hotkeyToggle()), "Hide/Show"));
    m_keysLayout1->addWidget(makeKey("Shift+Alt+" + vkToKeyName(cfg.hotkeyClear()), "Clear"));
    m_keysLayout1->addWidget(makeKey("Shift+Alt+" + vkToKeyName(cfg.hotkeyToggleBadges()), "All Keys ☰"));

    if (m_bottomHintLabel) {
        m_bottomHintLabel->setText(QString("[Shift+Alt+%1] Shuffle / All Keys Directory  |  [Shift+Alt+%2] Hide Keys")
            .arg(vkToKeyName(cfg.hotkeyToggleBadges()))
            .arg(vkToKeyName(cfg.hotkeyHideStrip())));
    }

    // Dynamically rebuild the All Keys HUD to reflect updated keys
    buildAllKeysHUD();

    // Dynamically update placeholder text in answer display
    if (m_answerDisplay) {
        m_answerDisplay->setPlaceholderText(
            QString("AI answer will appear here...\n\n"
                    "Essential Shortcuts:\n"
                    "  Shift+Alt+%1  →  Snap & Solve (AI Solution)\n"
                    "  Shift+Alt+%2  →  Capture Screenshot\n"
                    "  Shift+Alt+%3  →  Show / Hide Overlay\n"
                    "  Shift+Alt+%4  →  Clear Chat Output\n"
                    "  Shift+Alt+%5  →  All Keys Directory ☰ (Shuffle/View All Options)")
                .arg(vkToKeyName(cfg.hotkeyGetAnswer()))
                .arg(vkToKeyName(cfg.hotkeyScreenshot()))
                .arg(vkToKeyName(cfg.hotkeyToggle()))
                .arg(vkToKeyName(cfg.hotkeyClear()))
                .arg(vkToKeyName(cfg.hotkeyToggleBadges()))
        );
    }
}

void OverlayWindow::refreshSettings() {
    refreshKeyBadges();
    auto& cfg = AppConfig::instance();
    int w = cfg.overlayWidth();
    int h = cfg.overlayHeight();
    
    setFixedSize(w, h);
    m_container->setGeometry(0, 0, w, h);
    
    // Position might have changed too
    move(cfg.overlayX(), cfg.overlayY());
    
    // Opacity
    m_opacity = cfg.overlayOpacity();
    setWindowOpacity(m_opacity);
    
    update();
}

void OverlayWindow::moveLeft() {
    move(x() - 50, y());
    auto& cfg = AppConfig::instance();
    cfg.setOverlayPos(x(), y());
    cfg.save();
}

void OverlayWindow::moveRight() {
    move(x() + 50, y());
    auto& cfg = AppConfig::instance();
    cfg.setOverlayPos(x(), y());
    cfg.save();
}

void OverlayWindow::moveUp() {
    move(x(), y() - 50);
    auto& cfg = AppConfig::instance();
    cfg.setOverlayPos(x(), y());
    cfg.save();
}

void OverlayWindow::moveDown() {
    move(x(), y() + 50);
    auto& cfg = AppConfig::instance();
    cfg.setOverlayPos(x(), y());
    cfg.save();
}

// ─── AI response handlers ─────────────────────────────────────────────────────

void OverlayWindow::onAIStarted() {
    m_isLoading = true;
    m_loadingDots = 0;
    m_answerDisplay->setPlainText("⟳ Thinking");
    m_loadingTimer->start(350);
    showStatusMessage("AI is thinking...");
}

void OverlayWindow::onAIChunk(const QString& chunk) {
    if (m_isLoading) {
        m_isLoading = false;
        m_loadingTimer->stop();
        m_answerDisplay->clear();
        m_accumulatedResponse.clear();
    }

    // Accumulate raw response and re-render as formatted HTML
    m_accumulatedResponse += chunk;
    m_answerDisplay->setHtml(markdownToHtml(m_accumulatedResponse));

    // Auto-scroll to bottom
    QScrollBar* sb = m_answerDisplay->verticalScrollBar();
    if (sb) sb->setValue(sb->maximum());
}

void OverlayWindow::onAIComplete(const QString& full) {
    m_isLoading = false;
    m_loadingTimer->stop();
    m_accumulatedResponse = full;
    m_answerDisplay->setHtml(markdownToHtml(full));
    showStatusMessage("Done ✓");
}

void OverlayWindow::onAIError(const QString& err) {
    m_isLoading = false;
    m_loadingTimer->stop();
    
    // Auto API Failover
    auto& cfg = AppConfig::instance();
    int currentSlot = cfg.activeSlot();
    int nextSlot = (currentSlot + 1) % 10;
    bool found = false;
    for (int i = 0; i < 10; ++i) {
        if (!cfg.apiKeys()[nextSlot].isEmpty()) {
            found = true;
            break;
        }
        nextSlot = (nextSlot + 1) % 10;
    }
    
    if (found && nextSlot != currentSlot) {
        cfg.setActiveSlot(nextSlot);
        cfg.save();
        m_answerDisplay->setPlainText("⚠ Error: " + err + "\n\n↻ Auto-switched to API Slot " + QString::number(nextSlot + 1) + ".\nPress Get Answer (Shift+Alt+A) to retry.");
    } else {
        m_answerDisplay->setPlainText("⚠ Error: " + err);
    }
    
    showStatusMessage("Error!", true);
}

void OverlayWindow::animateLoading() {
    if (!m_isLoading) return;
    m_loadingDots = (m_loadingDots + 1) % 4;
    QString dots(m_loadingDots, '.');
    m_answerDisplay->setPlainText("⟳ Thinking" + dots);
}

// ─── Think Tag Stripper ───────────────────────────────────────────────────────

// Removes <think>...</think> blocks that reasoning models (DeepSeek, etc.) emit
QString OverlayWindow::stripThinkTags(const QString& text) {
    QString result = text;
    // Remove complete <think>...</think> blocks
    static QRegularExpression thinkRe("<think>[\\s\\S]*?</think>",
        QRegularExpression::CaseInsensitiveOption);
    result.replace(thinkRe, "");
    // Remove incomplete <think> at end (still streaming the thinking)
    int openIdx = result.lastIndexOf("<think>", -1, Qt::CaseInsensitive);
    if (openIdx != -1) {
        QString after = result.mid(openIdx);
        if (!after.contains("</think>", Qt::CaseInsensitive)) {
            result = result.left(openIdx);
        }
    }
    return result.trimmed();
}

// ─── Markdown → HTML Renderer ─────────────────────────────────────────────────

// Converts markdown to styled HTML for rich display in QTextBrowser
QString OverlayWindow::markdownToHtml(const QString& text) {
    QString cleaned = stripThinkTags(text);
    if (cleaned.isEmpty()) return "";

    // Helper: process inline markdown formatting within a line
    auto processInline = [](const QString& input) -> QString {
        QString s = input.toHtmlEscaped();

        // Bold: **text** and __text__ (must be before italic)
        s.replace(QRegularExpression("\\*\\*(.+?)\\*\\*"), "<b>\\1</b>");
        s.replace(QRegularExpression("__(.+?)__"), "<b>\\1</b>");

        // Italic: *text*
        s.replace(QRegularExpression("\\*(.+?)\\*"), "<i>\\1</i>");

        // Strikethrough: ~~text~~
        s.replace(QRegularExpression("~~(.+?)~~"), "<s>\\1</s>");

        // Inline code: `text`
        s.replace(QRegularExpression("`([^`]+)`"),
            "<code style=\"background:rgba(0,0,0,0.35); padding:1px 5px; "
            "border-radius:3px; font-family:Consolas,monospace; font-size:12px; "
            "color:#fbbf24;\">\\1</code>");

        return s;
    };

    QStringList lines = cleaned.split('\n');
    QString html;
    bool inCodeBlock = false;
    bool inUL = false;
    bool inOL = false;
    QString codeContent;
    static QRegularExpression orderedRe("^(\\d+)\\.\\s(.*)");

    for (const QString& rawLine : lines) {
        QString line = rawLine;
        if (line.endsWith('\r')) line.chop(1);
        QString trimmed = line.trimmed();

        // ── Code fence toggle ──
        if (trimmed.startsWith("```") || trimmed.startsWith("~~~")) {
            if (!inCodeBlock) {
                inCodeBlock = true;
                codeContent.clear();
            } else {
                inCodeBlock = false;
                html += QString("<pre style=\"background:rgba(0,0,0,0.4); padding:8px 10px; "
                    "border-radius:6px; font-family:Consolas,monospace; font-size:12px; "
                    "color:#e2e8f0; margin:6px 0; white-space:pre-wrap;\">%1</pre>")
                    .arg(codeContent.toHtmlEscaped());
            }
            continue;
        }
        if (inCodeBlock) {
            if (!codeContent.isEmpty()) codeContent += "\n";
            codeContent += line;
            continue;
        }

        // ── Detect list items ──
        bool isBullet = trimmed.startsWith("- ") || trimmed.startsWith("* ");
        QRegularExpressionMatch olMatch = orderedRe.match(trimmed);
        bool isOrdered = olMatch.hasMatch();

        // Close lists when line type changes
        if (inUL && !isBullet) { html += "</ul>"; inUL = false; }
        if (inOL && !isOrdered) { html += "</ol>"; inOL = false; }

        // ── Empty line ──
        if (trimmed.isEmpty()) {
            html += "<br>";
            continue;
        }

        // ── Headers ──
        if (trimmed.startsWith("#### ")) {
            html += QString("<p style=\"color:#cbd5e1; font-size:13px; font-weight:bold; "
                "margin:8px 0 3px;\">%1</p>").arg(processInline(trimmed.mid(5)));
        } else if (trimmed.startsWith("### ")) {
            html += QString("<p style=\"color:#e2e8f0; font-size:14px; font-weight:bold; "
                "margin:9px 0 3px;\">%1</p>").arg(processInline(trimmed.mid(4)));
        } else if (trimmed.startsWith("## ")) {
            html += QString("<p style=\"color:#f1f5f9; font-size:15px; font-weight:bold; "
                "margin:10px 0 4px;\">%1</p>").arg(processInline(trimmed.mid(3)));
        } else if (trimmed.startsWith("# ")) {
            html += QString("<p style=\"color:#ffffff; font-size:16px; font-weight:bold; "
                "margin:12px 0 5px;\">%1</p>").arg(processInline(trimmed.mid(2)));
        }
        // ── Blockquote ──
        else if (trimmed.startsWith("> ")) {
            html += QString("<div style=\"border-left:3px solid rgba(100,160,255,0.5); "
                "padding-left:10px; margin:5px 0; color:#94a3b8;\">%1</div>")
                .arg(processInline(trimmed.mid(2)));
        }
        // ── Bullet list ──
        else if (isBullet) {
            if (!inUL) { html += "<ul style=\"margin:4px 0; padding-left:18px;\">"; inUL = true; }
            html += QString("<li style=\"margin:2px 0; color:#e2e8f0;\">%1</li>")
                .arg(processInline(trimmed.mid(2)));
        }
        // ── Ordered list ──
        else if (isOrdered) {
            if (!inOL) { html += "<ol style=\"margin:4px 0; padding-left:18px;\">"; inOL = true; }
            html += QString("<li style=\"margin:2px 0; color:#e2e8f0;\">%1</li>")
                .arg(processInline(olMatch.captured(2)));
        }
        // ── Horizontal rule ──
        else if (trimmed == "---" || trimmed == "***" || trimmed == "___") {
            html += "<hr style=\"border:none; border-top:1px solid rgba(255,255,255,0.15); margin:8px 0;\">";
        }
        // ── Regular paragraph ──
        else {
            html += QString("<p style=\"margin:3px 0; color:#e2e8f0; line-height:1.5;\">%1</p>")
                .arg(processInline(trimmed));
        }
    }

    // Close any open lists
    if (inUL) html += "</ul>";
    if (inOL) html += "</ol>";

    // Handle unclosed code block (still streaming code)
    if (inCodeBlock && !codeContent.isEmpty()) {
        html += QString("<pre style=\"background:rgba(0,0,0,0.4); padding:8px 10px; "
            "border-radius:6px; font-family:Consolas,monospace; font-size:12px; "
            "color:#e2e8f0; margin:6px 0; white-space:pre-wrap;\">%1</pre>")
            .arg(codeContent.toHtmlEscaped());
    }

    return html;
}

// ─── Helper UI ────────────────────────────────────────────────────────────────

void OverlayWindow::updateScreenshotThumb(const QPixmap& px) {
    if (px.isNull()) return;

    // Remove placeholder if it's the first screenshot
    QLayout* layout = m_thumbGallery->layout();
    if (m_screenshots.size() == 1) {
        QLayoutItem* item = layout->takeAt(0);
        if (item) {
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
    }

    QLabel* thumb = new QLabel;
    thumb->setFixedSize(120, 75);
    thumb->setStyleSheet("border: 1px solid rgba(0, 229, 255, 0.35); border-radius: 6px; background: #0d121f;");
    
    QPixmap scaled = px.scaled(thumb->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    thumb->setPixmap(scaled);
    thumb->setAlignment(Qt::AlignCenter);
    
    layout->addWidget(thumb);
}

void OverlayWindow::showStatusMessage(const QString& msg, bool isError) {
    m_statusLabel->setText(msg);
    if (isError) {
        m_statusLabel->setStyleSheet("color: #ef4444; font-size: 11px; font-family: Consolas;");
    } else {
        m_statusLabel->setStyleSheet("color: rgba(0, 229, 255, 0.75); font-size: 11px; font-family: Consolas;");
    }

    // Clear after 4 seconds
    QTimer::singleShot(4000, this, [this]() {
        m_statusLabel->setText("Ready");
        m_statusLabel->setStyleSheet("color: rgba(0, 229, 255, 0.75); font-size: 11px; font-family: Consolas;");
    });
}

// ─── Mouse events (for move mode) ────────────────────────────────────────────

void OverlayWindow::mousePressEvent(QMouseEvent* event) {
    // Disabled: Mouse interaction not allowed
}

void OverlayWindow::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
#ifdef Q_OS_WIN
    HWND hwnd = (HWND)winId();
    
    // 1. Force Always on Top (Topmost) for fullscreen apps/games
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    
    // 2. Anti-Capture: Make the overlay invisible to its own screenshots
    // WDA_EXCLUDEFROMCAPTURE = 0x00000011
    SetWindowDisplayAffinity(hwnd, 0x00000011);
    
    // 3. Remove from Alt+Tab
    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle | WS_EX_TOOLWINDOW);
#endif
}

void OverlayWindow::mouseMoveEvent(QMouseEvent* event) {
    // Mouse passes through window completely
}

void OverlayWindow::mouseReleaseEvent(QMouseEvent* event) {
    // Disabled: Mouse interaction not allowed
}

bool OverlayWindow::eventFilter(QObject* obj, QEvent* event) {
    // All mouse events pass through - completely transparent overlay
    return false;
}

bool OverlayWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG") {
        MSG* msg = reinterpret_cast<MSG*>(message);
        
        // Handle WM_NCHITTEST to make window transparent to clicks
        if (msg->message == WM_NCHITTEST) {
            // Return HTTRANSPARENT to make clicks pass through to underlying windows
            *result = HTTRANSPARENT;
            return true;
        }

        // Handle WM_MOUSEACTIVATE to prevent focus steal when clicked
        if (msg->message == WM_MOUSEACTIVATE) {
            // MA_NOACTIVATE = stay inactive, don't discard the message (though it passes through anyway)
            *result = MA_NOACTIVATE;
            return true;
        }
    }
#endif
    return false;
}

void OverlayWindow::toggleVoiceRecord() {
    if (m_isRecordingAudio) {
        m_isRecordingAudio = false;
        if (m_transcribeTimer) {
            m_transcribeTimer->stop();
        }
        if (m_ai) {
            m_ai->cancelTranscriptionOnly();
        }
        m_answerDisplay->setPlainText("🎙️ Recording stopped.\n\n⚡ Processing and transcribing audio...");
        showStatusMessage("System Broker processing...");
        m_audioRecorder->stopRecording();
    } else {
        m_isRecordingAudio = true;
        m_recordTimer.start();
        m_answerDisplay->setPlainText("🎙️ Audio recording active...\n\nSpeak or play sound now.\n\n[Status: Recording...]");
        showStatusMessage("System Broker active...");
        
        // Generate temporary audio file path with randomized name
        QString tempPath = QDir::tempPath() + "/sys_audio_" + QUuid::createUuid().toString(QUuid::Id128).left(12) + ".wav";
        QFile::remove(tempPath);
        
        m_audioRecorder->startRecording(tempPath);

        // Start periodic transcription timer
        if (!m_transcribeTimer) {
            m_transcribeTimer = new QTimer(this);
            connect(m_transcribeTimer, &QTimer::timeout, this, &OverlayWindow::onTranscribeTimerTimeout);
        }
        m_transcribeTimer->start(2500); // 2.5 seconds chunk
    }
}

void OverlayWindow::stopVoiceRecordOnRelease() {
    // If we are recording and have held the key for longer than 400ms, stop recording.
    // Otherwise, treat it as a brief tap (to toggle).
    if (m_isRecordingAudio && m_recordTimer.isValid() && m_recordTimer.elapsed() > 400) {
        m_isRecordingAudio = false;
        if (m_transcribeTimer) {
            m_transcribeTimer->stop();
        }
        if (m_ai) {
            m_ai->cancelTranscriptionOnly();
        }
        m_answerDisplay->setPlainText("🎙️ Recording stopped.\n\n⚡ Processing and transcribing audio...");
        showStatusMessage("System Broker processing...");
        m_audioRecorder->stopRecording();
    }
}

void OverlayWindow::onTranscribeTimerTimeout() {
    if (!m_isRecordingAudio || !m_audioRecorder) return;
    
    QByteArray pcm = m_audioRecorder->getAccumulatedPcm();
    if (pcm.isEmpty()) return;
    
    // Save to intermediate chunk file
    QString chunkPath = QDir::tempPath() + "/sys_audio_transcribe_chunk.wav";
    QFile::remove(chunkPath);
    
    // Mixed PCM is 16kHz mono
    AudioRecorder::saveWavFile(chunkPath, pcm, 16000, 1);
    
    if (m_ai) {
        m_ai->transcribeAudioOnly(chunkPath);
    }
}

void OverlayWindow::onTranscriptionOnlyFinished(const QString& text) {
    if (m_isRecordingAudio && !text.isEmpty()) {
        m_answerDisplay->setPlainText("🎙️ [Recording...]\n\n" + text);
    }
}

void OverlayWindow::onRecordingFinished(const QString& filePath) {
    auto provider = AppConfig::instance().currentApiProvider();
    
    if (provider == "gemini") {
        // Read recording file to base64
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            m_answerDisplay->setPlainText("❌ Failed to read captured audio file.");
            showStatusMessage("Failed to read captured audio", true);
            return;
        }
        QByteArray audioData = file.readAll();
        file.close();
        
        // Cleanup temporary file
        QFile::remove(filePath);

        if (audioData.isEmpty()) {
            m_answerDisplay->setPlainText("❌ No audio captured (silence or error).");
            showStatusMessage("No audio captured", true);
            return;
        }

        QString base64Audio = QString::fromUtf8(audioData.toBase64());
        
        if (!m_ai) return;
        
        // Check if AI is busy
        if (m_ai->isBusy()) {
            m_answerDisplay->setPlainText("⚠️ AI is busy. Please wait for the current query to complete.");
            showStatusMessage("AI is busy... please wait", true);
            return;
        }

        m_answerDisplay->clear();
        showStatusMessage("Sending audio to Gemini...");
        m_ai->askWithImages(m_screenshots, "Solve / answer the question using the provided audio and screenshot(s).", base64Audio, "audio/wav");
    } else if (provider == "groq" || provider == "openai") {
        if (!m_ai) return;
        if (m_ai->isBusy()) {
            m_answerDisplay->setPlainText("⚠️ AI is busy. Please wait for the current query to complete.");
            showStatusMessage("AI is busy... please wait", true);
            QFile::remove(filePath);
            return;
        }
        m_answerDisplay->clear();
        showStatusMessage("Transcribing loopback audio...");
        m_ai->transcribeAudio(filePath, m_screenshots);
    } else {
        m_answerDisplay->setPlainText(QString("❌ Audio recording is not supported for provider: %1").arg(provider));
        showStatusMessage("Audio not supported for provider: " + provider, true);
        QFile::remove(filePath);
    }
}

void OverlayWindow::toggleBadgesVisibility() {
    if (m_contentStack) {
        int cur = m_contentStack->currentIndex();
        int next = (cur == 0) ? 1 : 0;
        m_contentStack->setCurrentIndex(next);
        // Auto-hide badge strip when HUD is open — no need to see both
        if (m_helpGroupsContainer)
            m_helpGroupsContainer->setVisible(next == 0);
        if (m_bottomHintLabel)
            m_bottomHintLabel->setVisible(next == 0);
        showStatusMessage(next == 1 ? "All Keys Directory Active" : "Ready");
        update();
        return;
    }

    if (m_helpGroupsContainer && m_bottomHintLabel) {
        bool currentlyVisible = m_helpGroupsContainer->isVisible();
        m_helpGroupsContainer->setVisible(!currentlyVisible);
        m_bottomHintLabel->setVisible(currentlyVisible);
        if (m_screenshotFrame) m_screenshotFrame->setVisible(!currentlyVisible);
        if (m_divider) m_divider->setVisible(!currentlyVisible);
        showStatusMessage(currentlyVisible ? "View collapsed" : "View expanded");
        update();
    }
}

void OverlayWindow::toggleHideStrip() {
    // Shift+Alt+L: hide the key badge strip so ONLY the AI answer is visible
    // Press again to bring badges back
    if (!m_helpGroupsContainer) return;
    bool isCurrentlyVisible = m_helpGroupsContainer->isVisible();
    m_helpGroupsContainer->setVisible(!isCurrentlyVisible);
    if (m_bottomHintLabel)
        m_bottomHintLabel->setVisible(!isCurrentlyVisible);
    showStatusMessage(isCurrentlyVisible ? "Keys hidden — clean view" : "Keys restored");
    update();
}

void OverlayWindow::copyScreenshotToClipboard() {
    if (!m_screenshots.isEmpty()) {
        QGuiApplication::clipboard()->setPixmap(m_screenshots.last());
        showStatusMessage("Copied to clipboard!");
    } else {
        showStatusMessage("No screenshot to copy", true);
    }
}

void OverlayWindow::doGhostWriter() {
    // Stop existing worker if active
    if (m_ghostWriterWorker) {
        m_ghostWriterWorker->requestStop();
        m_ghostWriterWorker->wait();
        delete m_ghostWriterWorker;
        m_ghostWriterWorker = nullptr;
    }

    QString response = m_accumulatedResponse;
    if (response.isEmpty()) {
        showStatusMessage("No AI response to type", true);
        return;
    }

    auto& cfg = AppConfig::instance();
    QString textToType = extractCodeBlock(response, cfg.ghostWriterSmartIndent());
    if (textToType.isEmpty()) {
        showStatusMessage("Empty code block", true);
        return;
    }

    // Hide overlay so focus is kept in the editor
    hide();
    m_visible = false;
    
    // Start worker thread
    m_ghostWriterWorker = new GhostWriterWorker(
        textToType,
        cfg.ghostWriterMinDelay(),
        cfg.ghostWriterMaxDelay(),
        this
    );
    
    connect(m_ghostWriterWorker, &QThread::finished, this, [this]() {
        showStatusMessage("Typing completed ✓");
    });
    
    m_ghostWriterWorker->start();
}

QString OverlayWindow::extractCodeBlock(const QString& fullText, bool smartIndent) {
    QString text = stripThinkTags(fullText);
    text.remove('\r');
    if (text.isEmpty()) return "";

    // Regular expression to match code blocks: ```[lang]\n<code>```
    QRegularExpression codeRe("```(?:[a-zA-Z0-9+#-]+)?\\n([\\s\\S]*?)```");
    QRegularExpressionMatchIterator it = codeRe.globalMatch(text);
    
    QStringList blocks;
    while (it.hasNext()) {
        blocks.append(it.next().captured(1));
    }

    QString selectedBlock;
    if (blocks.isEmpty()) {
        selectedBlock = text; // Fallback to raw text
    } else {
        // Heuristic: select the longest block (often the full coding solution)
        selectedBlock = blocks.last();
        for (const QString& block : blocks) {
            if (block.length() > selectedBlock.length()) {
                selectedBlock = block;
            }
        }
    }

    if (smartIndent) {
        // Strip common baseline indentation from the entire block
        QStringList lines = selectedBlock.split('\n');
        int minIndent = 9999;
        for (const QString& line : lines) {
            if (line.trimmed().isEmpty()) continue;
            int indent = 0;
            while (indent < line.length() && (line[indent] == ' ' || line[indent] == '\t')) {
                indent++;
            }
            if (indent < minIndent) {
                minIndent = indent;
            }
        }
        if (minIndent > 0 && minIndent < 100) {
            for (int i = 0; i < lines.size(); ++i) {
                if (lines[i].length() >= minIndent) {
                    // Check if it starts with spaces/tabs
                    bool onlySpace = true;
                    for (int j = 0; j < minIndent; ++j) {
                        if (lines[i][j] != ' ' && lines[i][j] != '\t') {
                            onlySpace = false;
                            break;
                        }
                    }
                    if (onlySpace) {
                        lines[i] = lines[i].mid(minIndent);
                    }
                }
            }
            selectedBlock = lines.join('\n');
        }
    }

    return selectedBlock.trimmed();
}

void OverlayWindow::updateCreditsBadge() {
    if (!m_creditsBadge) return;
    bool isPro = AccountManager::instance().isPro();
    if (isPro) {
        m_creditsBadge->setText(QString::fromUtf8("⚡ PRO"));
        m_creditsBadge->setStyleSheet("color: #00ff66; font-size: 10px; font-weight: 800; font-family: 'Consolas', monospace; background: rgba(0, 255, 102, 0.15); border: 1px solid rgba(0, 255, 102, 0.4); border-radius: 4px; padding: 1px 6px;");
        m_creditsBadge->setToolTip("Shadow Pro Active — Unlimited Solves");
    } else {
        int credits = AppConfig::instance().freeCredits();
        if (credits > 0) {
            m_creditsBadge->setText(QString::fromUtf8("🪙 %1 Solves").arg(credits));
            m_creditsBadge->setStyleSheet("color: #00e5ff; font-size: 10px; font-weight: 800; font-family: 'Consolas', monospace; background: rgba(0, 229, 255, 0.15); border: 1px solid rgba(0, 229, 255, 0.4); border-radius: 4px; padding: 1px 6px;");
        } else {
            m_creditsBadge->setText(QString::fromUtf8("🪙 0 Solves"));
            m_creditsBadge->setStyleSheet("color: #ff4757; font-size: 10px; font-weight: 800; font-family: 'Consolas', monospace; background: rgba(255, 71, 87, 0.15); border: 1px solid rgba(255, 71, 87, 0.4); border-radius: 4px; padding: 1px 6px;");
        }
        m_creditsBadge->setToolTip("Live AI Solve Credits Remaining");
    }
}

