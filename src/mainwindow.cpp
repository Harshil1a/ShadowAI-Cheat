#include "mainwindow.h"
#include "dashboard.h"
#include "accountmanager.h"
#include "appconfig.h"
#include <QPainter>
#include <QPainterPath>
#include <QGraphicsDropShadowEffect>
#include <QTimer>
#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QDesktopServices>
#include <QUrl>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

MainWindow::MainWindow(QWidget* parent) : QWidget(parent) {
    setWindowTitle("System Broker - Runtime Broker");
    setFixedSize(540, 520);
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_ShowWithoutActivating, false);

    setupUI();
    applyStyle();

    if (auto* scr = QGuiApplication::primaryScreen()) {
        move(scr->geometry().center() - QPoint(width() / 2, height() / 2));
    }
}

MainWindow::~MainWindow() {}

void MainWindow::setupUI() {
    QVBoxLayout* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // ── TITLE BAR ─────────────────────────────────────────────────────────────
    QWidget* titleBar = new QWidget(this);
    titleBar->setObjectName("customTitleBar");
    titleBar->setFixedHeight(44);
    
    QHBoxLayout* tbLayout = new QHBoxLayout(titleBar);
    tbLayout->setContentsMargins(14, 0, 8, 0);
    tbLayout->setSpacing(10);

    QLabel* tbIcon = new QLabel(QString::fromUtf8("❖"), titleBar);
    tbIcon->setObjectName("tbIcon");
    tbLayout->addWidget(tbIcon);

    m_titleLabel = new QLabel("SHADOW AI — ASSISTANT", titleBar);
    m_titleLabel->setObjectName("tbTitle");
    tbLayout->addWidget(m_titleLabel);

    tbLayout->addStretch();

    // Credits / Solves indicator button on top
    m_tbCreditsBtn = new QPushButton(titleBar);
    m_tbCreditsBtn->setObjectName("tbCreditsBtn");
    m_tbCreditsBtn->setCursor(Qt::PointingHandCursor);
    m_tbCreditsBtn->setFixedHeight(24);
    tbLayout->addWidget(m_tbCreditsBtn);

    m_tbRefreshBtn = new QPushButton("🔄", titleBar);
    m_tbRefreshBtn->setObjectName("tbRefreshBtn");
    m_tbRefreshBtn->setCursor(Qt::PointingHandCursor);
    m_tbRefreshBtn->setFixedSize(24, 24);
    m_tbRefreshBtn->setToolTip("Refresh & sync solve credits from cloud");
    m_tbRefreshBtn->setStyleSheet("background: rgba(0, 229, 255, 0.12); border: 1px solid rgba(0, 229, 255, 0.35); color: #00e5ff; font-weight: bold; border-radius: 4px; padding: 0px;");
    connect(m_tbRefreshBtn, &QPushButton::clicked, this, [this]() {
        m_tbRefreshBtn->setText("⏳");
        AccountManager::instance().fetchFreeCredits([this](bool, int) {
            m_tbRefreshBtn->setText("🔄");
            updateTopBarCredits();
        });
    });
    tbLayout->addWidget(m_tbRefreshBtn);

    // Quick Pro upgrade button on top
    m_tbProBtn = new QPushButton("⚡ PRO", titleBar);
    m_tbProBtn->setObjectName("tbProBtn");
    m_tbProBtn->setCursor(Qt::PointingHandCursor);
    m_tbProBtn->setFixedHeight(24);
    tbLayout->addWidget(m_tbProBtn);

    // — button: hide window, keep app running in tray
    m_hideBtn = new QPushButton("—", titleBar);
    m_hideBtn->setObjectName("tbHideBtn");
    m_hideBtn->setFixedSize(28, 22);
    m_hideBtn->setCursor(Qt::PointingHandCursor);
    tbLayout->addWidget(m_hideBtn);

    // × button: quit app completely
    m_closeBtn = new QPushButton("×", titleBar);
    m_closeBtn->setObjectName("tbCloseBtn");
    m_closeBtn->setFixedSize(28, 22);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    tbLayout->addWidget(m_closeBtn);

    rootLayout->addWidget(titleBar);

    // ── DASHBOARD CONTENT ──────────────────────────────────────────────────────
    m_dashboard = new Dashboard(this);
    m_dashboard->setWindowFlags(Qt::Widget);
    m_dashboard->setContentsMargins(0, 0, 0, 0);
    QWidget* childTitleBar = m_dashboard->findChild<QWidget*>("customTitleBar");
    if (childTitleBar) {
        childTitleBar->hide();
    }
    rootLayout->addWidget(m_dashboard, 1);

    // ── CONNECTIONS ────────────────────────────────────────────────────────────
    connect(m_tbCreditsBtn, &QPushButton::clicked, this, []() {
        if (!AccountManager::instance().isPro()) {
            AccountManager::instance().openWatchAdUrl();
        }
    });

    connect(m_tbProBtn, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl("https://shadow-ai-cheat.vercel.app/#pricing"));
    });

    connect(&AccountManager::instance(), &AccountManager::creditsUpdated, this, [this](int) {
        updateTopBarCredits();
    });

    connect(&AccountManager::instance(), &AccountManager::accountStateChanged, this, [this](bool, const QString&, bool) {
        updateTopBarCredits();
    });

    connect(m_hideBtn, &QPushButton::clicked, this, [this]() {
        emit m_dashboard->hideOverlay();
        hide();
    });

    connect(m_closeBtn, &QPushButton::clicked, this, [this]() {
        emit m_dashboard->quitApp();
    });

    updateTopBarCredits();
}

void MainWindow::paintEvent(QPaintEvent*) {
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
}

void MainWindow::mousePressEvent(QMouseEvent* event) {
    // Only allow dragging from the titlebar or empty space (not inside widgets)
    if (event->button() == Qt::LeftButton && event->position().y() < 44) {
        m_dragging = true;
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
    }
    QWidget::mousePressEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent* event) {
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragOffset);
    }
    QWidget::mouseMoveEvent(event);
}

void MainWindow::mouseReleaseEvent(QMouseEvent* event) {
    m_dragging = false;
    QWidget::mouseReleaseEvent(event);
}

void MainWindow::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
#ifdef Q_OS_WIN
    HWND hwnd = (HWND)winId();
    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    exStyle |= WS_EX_TOOLWINDOW;
    exStyle &= ~WS_EX_APPWINDOW;
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);
#endif
    updateTopBarCredits();
}

void MainWindow::updateTopBarCredits() {
    if (!m_tbCreditsBtn || !m_tbProBtn) return;
    bool isPro = AccountManager::instance().isPro();
    if (isPro) {
        m_tbCreditsBtn->setText(QString::fromUtf8("💎 PRO UNLIMITED"));
        m_tbCreditsBtn->setStyleSheet("background: rgba(0, 255, 102, 0.15); border: 1px solid rgba(0, 255, 102, 0.5); color: #00ff66; font-size: 11px; font-weight: bold; border-radius: 4px; padding: 2px 8px;");
        m_tbCreditsBtn->setToolTip("Pro Active: Unlimited Solves Enabled");
        m_tbProBtn->hide();
        if (m_tbRefreshBtn) m_tbRefreshBtn->hide();
    } else {
        int credits = AppConfig::instance().freeCredits();
        if (credits > 0) {
            m_tbCreditsBtn->setText(QString::fromUtf8("🪙 %1 Solves [+1 Ad]").arg(credits));
            m_tbCreditsBtn->setStyleSheet("background: rgba(0, 229, 255, 0.15); border: 1px solid rgba(0, 229, 255, 0.5); color: #00e5ff; font-size: 11px; font-weight: bold; border-radius: 4px; padding: 2px 8px;");
        } else {
            m_tbCreditsBtn->setText(QString::fromUtf8("🪙 0 Solves [Get +1]"));
            m_tbCreditsBtn->setStyleSheet("background: rgba(255, 71, 87, 0.18); border: 1px solid rgba(255, 71, 87, 0.6); color: #ff4757; font-size: 11px; font-weight: bold; border-radius: 4px; padding: 2px 8px;");
        }
        m_tbCreditsBtn->setToolTip("Available Solve Credits. Click to complete a sponsor task on LootLabs (+1 solve).");
        m_tbProBtn->show();
        m_tbProBtn->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #ff9900, stop:1 #ff5500); border: none; color: #ffffff; font-size: 11px; font-weight: bold; border-radius: 4px; padding: 2px 8px;");
        m_tbProBtn->setToolTip("Upgrade to Shadow PRO for Unlimited Solves");
        if (m_tbRefreshBtn) m_tbRefreshBtn->show();
    }
}


void MainWindow::applyStyle() {
    setStyleSheet(R"(
        MainWindow {
            background: transparent;
        }

        /* ═══ Custom Title Bar ═══ */
        QWidget#customTitleBar {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 rgba(10, 20, 38, 0.95), stop:1 rgba(6, 12, 24, 0.98));
            border-bottom: 1px solid rgba(0, 229, 255, 0.18);
        }
        QLabel#tbIcon {
            color: #00e5ff;
            font-size: 16px;
            font-weight: bold;
            background: transparent;
            margin-right: 6px;
        }
        QLabel#tbTitle {
            color: #00e5ff;
            font-family: 'Segoe UI', sans-serif;
            font-size: 13px;
            font-weight: bold;
            letter-spacing: 1.5px;
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
    )");
}
