#include "dashboard.h"
#include "accountmanager.h"
#include <QApplication>
#include <QCloseEvent>
#include <QShowEvent>
#include <QEvent>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QPainter>
#include <QPainterPath>
#include <QGraphicsDropShadowEffect>

Dashboard::Dashboard(QWidget* parent) : QWidget(parent) {
    m_nam = new QNetworkAccessManager(this);
    setWindowTitle("System Broker - Runtime Broker");
    setFixedSize(540, 520);
    // Frameless — we draw our own title bar with custom — and × buttons
    setWindowFlags(Qt::Window | Qt::Tool | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_ShowWithoutActivating, true);

    setupUI();
    applyStyle();

    m_settingsWidget = new SettingsWindow(nullptr);
    m_settingsWidget->setWindowFlags(Qt::Window | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint);
    m_settingsWidget->setWindowTitle("System Broker — Settings - Runtime Broker");

    QTimer::singleShot(500, this, &Dashboard::checkInternet);
    connect(m_nam, &QNetworkAccessManager::finished, this, &Dashboard::onInternetResult);
}

void Dashboard::setupUI() {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(40, 40, 40, 32);
    layout->setSpacing(0);

    // ── CUSTOM TITLE BAR ─────────────────────────────────────────────
    QWidget* titleBar = new QWidget(this);
    titleBar->setObjectName("customTitleBar");
    titleBar->setFixedHeight(36);
    QHBoxLayout* tbLayout = new QHBoxLayout(titleBar);
    tbLayout->setContentsMargins(14, 0, 8, 0);
    tbLayout->setSpacing(4);

    QLabel* tbTitle = new QLabel(QString::fromUtf8("❖  RUNTIME BROKER"), titleBar);
    tbTitle->setObjectName("tbTitle");
    tbLayout->addWidget(tbTitle);
    tbLayout->addStretch();

    // — button: hide overlay only, keep app running in tray
    m_hideBtn = new QPushButton("—", titleBar);
    m_hideBtn->setObjectName("tbHideBtn");
    m_hideBtn->setFixedSize(28, 22);
    m_hideBtn->setToolTip("Hide Overlay (keep app running)");
    m_hideBtn->setCursor(Qt::PointingHandCursor);
    tbLayout->addWidget(m_hideBtn);

    // × button: quit app completely
    m_closeBtn = new QPushButton("×", titleBar);
    m_closeBtn->setObjectName("tbCloseBtn");
    m_closeBtn->setFixedSize(28, 22);
    m_closeBtn->setToolTip("Quit Application");
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    tbLayout->addWidget(m_closeBtn);

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(titleBar);

    // Inner content with padding
    QWidget* content = new QWidget(this);
    QVBoxLayout* cLayout = new QVBoxLayout(content);
    cLayout->setContentsMargins(40, 20, 40, 32);
    cLayout->setSpacing(0);
    layout->addWidget(content, 1);

    // ── DIAMOND ICON ─────────────────────────────────────────────────
    QLabel* iconLabel = new QLabel(QString::fromUtf8("❖"), content);
    iconLabel->setObjectName("diamondIcon");
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setFixedHeight(44);
    QGraphicsDropShadowEffect* iconGlow = new QGraphicsDropShadowEffect(iconLabel);
    iconGlow->setColor(QColor(0, 229, 255, 180));
    iconGlow->setBlurRadius(30);
    iconGlow->setOffset(0, 0);
    iconLabel->setGraphicsEffect(iconGlow);
    cLayout->addWidget(iconLabel);
    cLayout->addSpacing(4);

    // ── TITLE ────────────────────────────────────────────────────────
    m_titleLabel = new QLabel("RUNTIME BROKER", content);
    m_titleLabel->setObjectName("mainTitle");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setFixedHeight(34);
    QGraphicsDropShadowEffect* titleGlow = new QGraphicsDropShadowEffect(m_titleLabel);
    titleGlow->setColor(QColor(0, 229, 255, 120));
    titleGlow->setBlurRadius(22);
    titleGlow->setOffset(0, 2);
    m_titleLabel->setGraphicsEffect(titleGlow);
    cLayout->addWidget(m_titleLabel);
    cLayout->addSpacing(2);

    // ── SUBTITLE ─────────────────────────────────────────────────────
    QLabel* subtitle = new QLabel("System Configuration", content);
    subtitle->setObjectName("subtitle");
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setFixedHeight(20);
    cLayout->addWidget(subtitle);
    cLayout->addSpacing(8);

    // ── ACCOUNT & LICENSE BADGE ───────────────────────────────────────
    QWidget* accRow = new QWidget(content);
    QHBoxLayout* accLayout = new QHBoxLayout(accRow);
    accLayout->setContentsMargins(0, 0, 0, 0);
    accLayout->setSpacing(8);

    m_accountBadge = new QLabel(accRow);
    m_accountBadge->setObjectName("accountBadge");
    m_accountBadge->setAlignment(Qt::AlignCenter);

    m_loginBtn = new QPushButton("Login", accRow);
    m_loginBtn->setObjectName("accLoginBtn");
    m_loginBtn->setFixedSize(65, 24);
    m_loginBtn->setCursor(Qt::PointingHandCursor);

    m_proBtn = new QPushButton("Pro ⚡", accRow);
    m_proBtn->setObjectName("accProBtn");
    m_proBtn->setFixedSize(60, 24);
    m_proBtn->setCursor(Qt::PointingHandCursor);

    accLayout->addStretch();
    accLayout->addWidget(m_accountBadge);
    accLayout->addWidget(m_loginBtn);
    accLayout->addWidget(m_proBtn);
    accLayout->addStretch();

    cLayout->addWidget(accRow);
    cLayout->addSpacing(14);

    refreshAccountUI();

    connect(m_loginBtn, &QPushButton::clicked, this, [this]() {
        if (AccountManager::instance().isLoggedIn()) {
            AccountManager::instance().logout();
        } else {
            AccountManager::instance().startGoogleLogin();
        }
    });

    connect(m_proBtn, &QPushButton::clicked, this, [this]() {
        openSettingsPage();
    });

    connect(&AccountManager::instance(), &AccountManager::accountStateChanged, this, [this]() {
        refreshAccountUI();
    });

    // ── STATUS ───────────────────────────────────────────────────────
    m_statusLabel = new QLabel(QString::fromUtf8("⬤  Status: Stopped (Offline)"), content);
    m_statusLabel->setObjectName("statusLabel");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setFixedHeight(30);
    cLayout->addWidget(m_statusLabel);
    cLayout->addSpacing(18);

    // ── START/STOP ASSISTANT BUTTON ──────────────────────────────────
    m_toggleBtn = new QPushButton(QString::fromUtf8("▶  START ASSISTANT"), content);
    m_toggleBtn->setObjectName("toggleBtn");
    m_toggleBtn->setFixedSize(360, 54);
    m_toggleBtn->setCursor(Qt::PointingHandCursor);
    cLayout->addWidget(m_toggleBtn, 0, Qt::AlignCenter);
    cLayout->addSpacing(12);

    // ── SETTINGS BUTTON ──────────────────────────────────────────────
    m_settingsBtn = new QPushButton(QString::fromUtf8("⚙  SETTINGS  ·  API KEYS"), content);
    m_settingsBtn->setObjectName("settingsBtn");
    m_settingsBtn->setFixedSize(360, 44);
    m_settingsBtn->setCursor(Qt::PointingHandCursor);
    cLayout->addWidget(m_settingsBtn, 0, Qt::AlignCenter);
    cLayout->addSpacing(28);

    // ── DIVIDER ──────────────────────────────────────────────────────
    QWidget* divider = new QWidget(content);
    divider->setObjectName("divider3d");
    divider->setFixedHeight(2);
    divider->setFixedWidth(300);
    cLayout->addWidget(divider, 0, Qt::AlignCenter);
    cLayout->addSpacing(20);

    // ── EXIT BUTTON ──────────────────────────────────────────────────
    m_exitBtn = new QPushButton(QString::fromUtf8("✕  EXIT APPLICATION"), content);
    m_exitBtn->setObjectName("exitBtn");
    m_exitBtn->setCursor(Qt::PointingHandCursor);
    m_exitBtn->setFixedSize(220, 32);
    cLayout->addWidget(m_exitBtn, 0, Qt::AlignCenter);

    // ── CONNECTIONS ──────────────────────────────────────────────────
    // — title bar button: hide overlay, keep app in tray
    connect(m_hideBtn, &QPushButton::clicked, this, [this]() {
        emit hideOverlay();   // hide the overlay
        hide();               // hide this dashboard too
    });
    // × title bar button: quit the whole app
    connect(m_closeBtn, &QPushButton::clicked, this, [this]() {
        emit quitApp();
    });
    connect(m_toggleBtn, &QPushButton::clicked, this, [this]() {
        if (!AccountManager::instance().isLoggedIn()) {
            m_statusLabel->setText(QString::fromUtf8("🔒 Google Sign-In Required to Activate Assistant"));
            m_statusLabel->setStyleSheet("color: #ffa502; font-size: 11px; font-weight: bold; font-family: 'Consolas', monospace;");
            AccountManager::instance().startGoogleLogin();
            return;
        }
        emit toggleOverlay();
    });
    connect(m_settingsBtn, &QPushButton::clicked, this, [this]() {
        openSettingsPage();
    });
    // Exit Application button = quit completely
    connect(m_exitBtn, &QPushButton::clicked, this, [this]() {
        emit quitApp();
    });
}

void Dashboard::openSettingsPage() {
    if (m_settingsWidget) {
        m_settingsWidget->show();
        m_settingsWidget->raise();
        m_settingsWidget->activateWindow();
    }
}

void Dashboard::updateStatus(bool active, bool visible) {
    m_isOverlayRunning = active;
    m_toggleBtn->setEnabled(true);
    if (!m_isOnline) {
        m_statusLabel->setText(QString::fromUtf8("⚠  Status: No Internet Detected"));
        m_statusLabel->setStyleSheet(
            "color: #f59e0b; font-size: 13px; font-family: 'Consolas', monospace;"
            "background: rgba(245, 158, 11, 0.07); border: 1px solid rgba(245, 158, 11, 0.25); border-radius: 6px;"
        );
    } else if (active) {
        if (visible) {
            m_statusLabel->setText(QString::fromUtf8("⬤  Status: Active (Visible)"));
        } else {
            m_statusLabel->setText(QString::fromUtf8("⬤  Status: Active (Hidden)"));
        }
        m_statusLabel->setStyleSheet(
            "color: #00e5ff; font-size: 13px; font-family: 'Consolas', monospace;"
            "background: rgba(0, 229, 255, 0.07); border: 1px solid rgba(0, 229, 255, 0.18); border-radius: 6px;"
        );
    } else {
        m_statusLabel->setText(QString::fromUtf8("⬤  Status: Stopped (Offline)"));
        m_statusLabel->setStyleSheet(
            "color: #8b9bb4; font-size: 13px; font-family: 'Consolas', monospace;"
            "background: rgba(100, 130, 170, 0.06); border: 1px solid rgba(100, 130, 170, 0.12); border-radius: 6px;"
        );
    }
    if (active) {
        m_toggleBtn->setText(QString::fromUtf8("■  STOP ASSISTANT"));
    } else {
        m_toggleBtn->setText(QString::fromUtf8("▶  START ASSISTANT"));
    }
}

void Dashboard::checkInternet() {
    // Don't disable the button during check — just show status
    m_statusLabel->setText(QString::fromUtf8("⟳  Status: Checking connection..."));
    // Use a reliable lightweight check
    QNetworkRequest req(QUrl("https://www.google.com/generate_204"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_nam->get(req);
    QTimer::singleShot(4000, reply, [reply]() {
        if (reply->isRunning()) reply->abort();
    });
}

void Dashboard::onInternetResult(QNetworkReply* reply) {
    m_isOnline = (reply->error() == QNetworkReply::NoError);
    reply->deleteLater();
    updateStatus(m_isOverlayRunning);
}

void Dashboard::closeEvent(QCloseEvent* event) {
    // × on title bar = quit the whole app
    emit quitApp();
    event->accept();
}

void Dashboard::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
    }
    QWidget::mousePressEvent(event);
}
void Dashboard::mouseMoveEvent(QMouseEvent* event) {
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragOffset);
    }
    QWidget::mouseMoveEvent(event);
}
void Dashboard::mouseReleaseEvent(QMouseEvent* event) {
    m_dragging = false;
    QWidget::mouseReleaseEvent(event);
}


void Dashboard::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
#ifdef Q_OS_WIN
    HWND hwnd = (HWND)winId();
    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    exStyle |= WS_EX_TOOLWINDOW;
    exStyle &= ~WS_EX_APPWINDOW;
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);
#endif
}

void Dashboard::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();

    // Outer background
    QLinearGradient outerBg(0, 0, 0, h);
    outerBg.setColorAt(0.0, QColor(4, 8, 18));
    outerBg.setColorAt(0.5, QColor(6, 12, 26));
    outerBg.setColorAt(1.0, QColor(3, 6, 14));
    p.fillRect(rect(), outerBg);

    // 3D raised panel
    int m = 16;
    int rad = 16;
    QRectF panelRect(m, m, w - 2 * m, h - 2 * m);
    QPainterPath panelPath;
    panelPath.addRoundedRect(panelRect, rad, rad);

    QLinearGradient panelFill(m, m, m, h - m);
    panelFill.setColorAt(0.0, QColor(14, 24, 44));
    panelFill.setColorAt(0.35, QColor(11, 20, 38));
    panelFill.setColorAt(0.7, QColor(9, 16, 32));
    panelFill.setColorAt(1.0, QColor(7, 12, 26));
    p.fillPath(panelPath, panelFill);

    // Top accent line
    QLinearGradient topHighlight(m + rad, m, w - m - rad, m);
    topHighlight.setColorAt(0.0, Qt::transparent);
    topHighlight.setColorAt(0.2, QColor(139, 92, 246, 70));
    topHighlight.setColorAt(0.5, QColor(0, 229, 255, 120));
    topHighlight.setColorAt(0.8, QColor(139, 92, 246, 70));
    topHighlight.setColorAt(1.0, Qt::transparent);
    p.setPen(QPen(QBrush(topHighlight), 2));
    p.drawLine(QPointF(m + rad, m + 0.5), QPointF(w - m - rad, m + 0.5));

    // Edges for 3D
    p.setPen(QPen(QColor(0, 229, 255, 18), 1));
    p.drawLine(QPointF(m + 0.5, m + rad), QPointF(m + 0.5, h - m - rad));
    p.setPen(QPen(QColor(0, 0, 0, 70), 1));
    p.drawLine(QPointF(m + rad, h - m - 0.5), QPointF(w - m - rad, h - m - 0.5));
    p.setPen(QPen(QColor(0, 0, 0, 40), 1));
    p.drawLine(QPointF(w - m - 0.5, m + rad), QPointF(w - m - 0.5, h - m - rad));

    // Panel outline
    p.setPen(QPen(QColor(0, 229, 255, 16), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(panelRect, rad, rad);

    // Corner dots
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 229, 255, 35));
    int d = m + 8;
    p.drawEllipse(QPointF(d, d), 2.5, 2.5);
    p.drawEllipse(QPointF(w - d, d), 2.5, 2.5);
    p.drawEllipse(QPointF(d, h - d), 2.5, 2.5);
    p.drawEllipse(QPointF(w - d, h - d), 2.5, 2.5);
}

void Dashboard::applyStyle() {
    setStyleSheet(R"(
        Dashboard {
            background: transparent;
        }

        /* ═══ Custom Title Bar ═══ */
        QWidget#customTitleBar {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 rgba(0,229,255,0.10), stop:1 rgba(0,0,0,0.30));
            border-bottom: 1px solid rgba(0,229,255,0.18);
        }
        QLabel#tbTitle {
            color: #00e5ff;
            font-family: 'Consolas', monospace;
            font-size: 11px;
            font-weight: bold;
            letter-spacing: 2px;
            background: transparent;
        }
        QPushButton#tbHideBtn {
            background: transparent;
            border: 1px solid rgba(0,229,255,0.15);
            border-radius: 3px;
            color: #8b9bb4;
            font-size: 14px;
            font-weight: bold;
        }
        QPushButton#tbHideBtn:hover {
            background: rgba(0,229,255,0.12);
            border: 1px solid rgba(0,229,255,0.4);
            color: #00e5ff;
        }
        QPushButton#tbCloseBtn {
            background: transparent;
            border: 1px solid rgba(255,80,80,0.15);
            border-radius: 3px;
            color: #ff6b6b;
            font-size: 14px;
            font-weight: bold;
        }
        QPushButton#tbCloseBtn:hover {
            background: rgba(255,60,60,0.18);
            border: 1px solid rgba(255,80,80,0.5);
            color: #ff3333;
        }

        /* ═══ Diamond Icon ═══ */
        QLabel#diamondIcon {
            color: #00e5ff;
            font-size: 30px;
            font-weight: bold;
            background: transparent;
        }

        /* ═══ Title ═══ */
        QLabel#mainTitle {
            color: #00e5ff;
            font-size: 24px;
            font-weight: 900;
            letter-spacing: 4px;
            background: transparent;
        }

        /* ═══ Subtitle ═══ */
        QLabel#subtitle {
            color: #556680;
            font-size: 11px;
            font-weight: 600;
            letter-spacing: 2px;
            background: transparent;
        }

        /* ═══ Status ═══ */
        QLabel#statusLabel {
            color: #8b9bb4;
            font-size: 13px;
            font-family: 'Consolas', monospace;
            background: rgba(100, 130, 170, 0.06);
            border: 1px solid rgba(100, 130, 170, 0.12);
            border-radius: 6px;
            padding: 4px 12px;
        }

        /* ═══ Divider ═══ */
        QWidget#divider3d {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 rgba(0, 0, 0, 0.5),
                stop:0.5 rgba(0, 229, 255, 0.15),
                stop:1 rgba(255, 255, 255, 0.04));
        }

        /* ═══ LAUNCH BUTTON ═══ */
        QPushButton#toggleBtn {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #0d2a3a, stop:0.45 #0a2030, stop:1 #081828);
            border-top: 2px solid rgba(0, 229, 255, 0.7);
            border-left: 2px solid rgba(0, 229, 255, 0.45);
            border-right: 2px solid rgba(0, 140, 170, 0.25);
            border-bottom: 3px solid rgba(0, 80, 110, 0.35);
            border-radius: 8px;
            color: #00e5ff;
            font-family: 'Segoe UI', sans-serif;
            font-size: 14px;
            font-weight: bold;
            letter-spacing: 2px;
        }
        QPushButton#toggleBtn:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #143848, stop:0.5 #0f2e3c, stop:1 #0b2430);
            border-top: 2px solid rgba(0, 229, 255, 0.9);
            border-left: 2px solid rgba(0, 229, 255, 0.65);
            border-right: 2px solid rgba(0, 180, 220, 0.4);
            border-bottom: 3px solid rgba(0, 140, 170, 0.45);
            color: #ffffff;
        }
        QPushButton#toggleBtn:pressed {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #081828, stop:0.5 #0c2535, stop:1 #10303f);
            border-top: 2px solid rgba(0, 120, 160, 0.25);
            border-left: 2px solid rgba(0, 120, 160, 0.2);
            border-right: 2px solid rgba(0, 229, 255, 0.55);
            border-bottom: 3px solid rgba(0, 229, 255, 0.7);
            color: #b0f0ff;
        }
        QPushButton#toggleBtn:disabled {
            border: 2px solid rgba(255, 255, 255, 0.06);
            color: rgba(255, 255, 255, 0.15);
            background: #0a1420;
        }

        /* ═══ SETTINGS BUTTON ═══ */
        QPushButton#settingsBtn {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #0a1e2e, stop:0.5 #091828, stop:1 #071422);
            border-top: 1px solid rgba(0, 229, 255, 0.35);
            border-left: 1px solid rgba(0, 229, 255, 0.22);
            border-right: 1px solid rgba(0, 140, 170, 0.14);
            border-bottom: 2px solid rgba(0, 80, 120, 0.2);
            border-radius: 7px;
            color: #8b9bb4;
            font-family: 'Segoe UI', sans-serif;
            font-size: 12px;
            font-weight: bold;
            letter-spacing: 1.5px;
        }
        QPushButton#settingsBtn:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #102838, stop:1 #0c1e2e);
            border-top: 1px solid rgba(0, 229, 255, 0.55);
            border-left: 1px solid rgba(0, 229, 255, 0.38);
            border-right: 1px solid rgba(0, 180, 220, 0.28);
            border-bottom: 2px solid rgba(0, 140, 170, 0.3);
            color: #00e5ff;
        }
        QPushButton#settingsBtn:pressed {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #081422, stop:1 #0e2838);
            border-top: 1px solid rgba(0, 140, 170, 0.18);
            border-bottom: 2px solid rgba(0, 229, 255, 0.45);
            color: #b0f0ff;
        }

        /* ═══ EXIT BUTTON ═══ */
        QPushButton#exitBtn {
            background: transparent;
            border: none;
            color: #ff6b6b;
            font-family: 'Segoe UI', sans-serif;
            font-size: 11px;
            font-weight: bold;
            letter-spacing: 1.5px;
        }
        QPushButton#exitBtn:hover {
            color: #ff4444;
            text-decoration: underline;
        }

        /* ═══ ACCOUNT BUTTONS ═══ */
        QPushButton#accLoginBtn {
            background: rgba(0, 229, 255, 0.08);
            border: 1px solid rgba(0, 229, 255, 0.3);
            color: #00e5ff;
            border-radius: 4px;
            font-size: 10px;
            font-weight: bold;
        }
        QPushButton#accLoginBtn:hover {
            background: rgba(0, 229, 255, 0.2);
            border-color: #00e5ff;
        }
    )");
}

void Dashboard::refreshAccountUI() {
    if (!m_accountBadge || !m_loginBtn || !m_proBtn) return;

    bool loggedIn = AccountManager::instance().isLoggedIn();
    bool isPro = AccountManager::instance().isPro();
    QString email = AccountManager::instance().userEmail();

    email = QUrl::fromPercentEncoding(email.toUtf8()).trimmed();
    if (email.contains("operator@gmail.com", Qt::CaseInsensitive) || email.isEmpty()) {
        loggedIn = false;
    }

    if (!loggedIn) {
        m_accountBadge->setText(QString::fromUtf8("🔒 SIGN-IN REQUIRED"));
        m_accountBadge->setStyleSheet("color: #ffa502; font-size: 11px; font-weight: bold; font-family: 'Consolas', monospace; border: 1px dashed rgba(255,165,2,0.5); padding: 2px 8px; border-radius: 4px;");
        m_loginBtn->setText("Sign in with Google");
        m_loginBtn->setStyleSheet("background: #00ff66; color: #030805; font-weight: 800; font-size: 11px; border-radius: 4px; padding: 4px 12px;");
        m_loginBtn->setToolTip("Mandatory: Sign in with your Google account to unlock assistant");

        if (!isOverlayRunning()) {
            m_statusLabel->setText(QString::fromUtf8("🔒 Status: Locked (Google Login Required)"));
            m_statusLabel->setStyleSheet("color: #ffa502; font-size: 11px; font-weight: bold; font-family: 'Consolas', monospace;");
            m_toggleBtn->setText(QString::fromUtf8("🔒 SIGN IN WITH GOOGLE TO START"));
            m_toggleBtn->setStyleSheet("background: rgba(255, 165, 2, 0.12); color: #ffa502; border: 1px solid #ffa502; font-size: 13px; font-weight: bold; border-radius: 6px;");
        }
    } else {
        QString shortEmail = email.length() > 22 ? email.left(19) + "..." : email;
        m_accountBadge->setText(QString("👤 %1").arg(shortEmail));
        m_accountBadge->setStyleSheet("color: #00ff66; font-size: 11px; font-family: 'Consolas', monospace; font-weight: bold;");
        m_loginBtn->setText("Logout");
        m_loginBtn->setStyleSheet("background: rgba(255, 71, 87, 0.15); color: #ff4757; border: 1px solid #ff4757; font-size: 10px; font-weight: bold; border-radius: 4px; padding: 4px 10px;");
        m_loginBtn->setToolTip("Click to sign out");

        if (!isOverlayRunning()) {
            m_statusLabel->setText(QString::fromUtf8("⬤  Status: Stopped (Offline)"));
            m_statusLabel->setStyleSheet("color: #7ca88e; font-size: 11px; font-family: 'Consolas', monospace;");
            m_toggleBtn->setText(QString::fromUtf8("▶  START ASSISTANT"));
            m_toggleBtn->setStyleSheet("background: #00ff66; color: #030508; font-size: 14px; font-weight: bold; border-radius: 6px;");
        }
    }

    if (isPro) {
        m_proBtn->setText(QString::fromUtf8("PRO ACTIVE ✓"));
        m_proBtn->setStyleSheet("background: rgba(0, 255, 102, 0.25); color: #00ff66; border: 1px solid #00ff66; font-size: 10px; font-weight: bold; border-radius: 4px;");
        m_proBtn->setToolTip("Pro Active [Unlimited Access]");
    } else {
        m_proBtn->setText(QString::fromUtf8("UPGRADE ⚡"));
        m_proBtn->setStyleSheet("background: rgba(0, 229, 255, 0.1); color: #00e5ff; border: 1px solid rgba(0, 229, 255, 0.4); font-size: 10px; font-weight: bold; border-radius: 4px;");
        m_proBtn->setToolTip("Click to activate Pro license");
    }
}

