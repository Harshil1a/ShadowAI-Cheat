#include "vmbrowser.h"
#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QTimer>
#include <QUrl>
#include <QNetworkRequest>
#include <QStyle>
#include <QCommonStyle>

VMBrowserOverlay::VMBrowserOverlay(QWidget* parent) : QWidget(parent) {
    m_nam = new QNetworkAccessManager(this);
    
    setupUI();
    applyStyle();

    onNewTab(); // Create initial tab
}

VMBrowserOverlay::~VMBrowserOverlay() {}

void VMBrowserOverlay::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    // ── TOP NAVIGATION BAR ───────────────────────────────────────────────────
    QWidget* navBar = new QWidget(this);
    navBar->setObjectName("navBar");
    QHBoxLayout* navLayout = new QHBoxLayout(navBar);
    navLayout->setContentsMargins(6, 6, 6, 6);
    navLayout->setSpacing(6);

    m_backBtn = new QPushButton("◀", navBar);
    m_backBtn->setObjectName("navBtn");
    m_backBtn->setFixedSize(28, 28);
    navLayout->addWidget(m_backBtn);

    m_forwardBtn = new QPushButton("▶", navBar);
    m_forwardBtn->setObjectName("navBtn");
    m_forwardBtn->setFixedSize(28, 28);
    navLayout->addWidget(m_forwardBtn);

    m_refreshBtn = new QPushButton("⟳", navBar);
    m_refreshBtn->setObjectName("navBtn");
    m_refreshBtn->setFixedSize(28, 28);
    navLayout->addWidget(m_refreshBtn);

    m_addressBar = new QLineEdit(navBar);
    m_addressBar->setObjectName("addressBar");
    m_addressBar->setPlaceholderText("Enter URL or search Google...");
    m_addressBar->setFixedHeight(28);
    navLayout->addWidget(m_addressBar, 1);

    m_searchBtn = new QPushButton("Search", navBar);
    m_searchBtn->setObjectName("searchBtn");
    m_searchBtn->setFixedHeight(28);
    m_searchBtn->setCursor(Qt::PointingHandCursor);
    navLayout->addWidget(m_searchBtn);

    m_newTabBtn = new QPushButton("+", navBar);
    m_newTabBtn->setObjectName("navBtn");
    m_newTabBtn->setFixedSize(28, 28);
    m_newTabBtn->setToolTip("Open New Tab");
    navLayout->addWidget(m_newTabBtn);

    mainLayout->addWidget(navBar);

    // ── SCREENSHOT ACTIONS BAR ───────────────────────────────────────────────
    QWidget* capBar = new QWidget(this);
    capBar->setObjectName("capBar");
    QHBoxLayout* capLayout = new QHBoxLayout(capBar);
    capLayout->setContentsMargins(4, 4, 4, 4);
    capLayout->setSpacing(10);

    QLabel* capLabel = new QLabel("SCREENSHOT ENGINE:", capBar);
    capLabel->setObjectName("capLabel");
    capLayout->addWidget(capLabel);

    m_capFullBtn = new QPushButton("📸 FULL SCREEN", capBar);
    m_capFullBtn->setObjectName("capBtn");
    m_capFullBtn->setFixedHeight(26);
    m_capFullBtn->setCursor(Qt::PointingHandCursor);
    capLayout->addWidget(m_capFullBtn);

    m_capWinBtn = new QPushButton("🪟 CAPTURE WINDOW", capBar);
    m_capWinBtn->setObjectName("capBtn");
    m_capWinBtn->setFixedHeight(26);
    m_capWinBtn->setCursor(Qt::PointingHandCursor);
    capLayout->addWidget(m_capWinBtn);

    m_capRegBtn = new QPushButton("📐 SELECT REGION", capBar);
    m_capRegBtn->setObjectName("capBtn");
    m_capRegBtn->setFixedHeight(26);
    m_capRegBtn->setCursor(Qt::PointingHandCursor);
    capLayout->addWidget(m_capRegBtn);

    capLayout->addStretch();
    mainLayout->addWidget(capBar);

    // ── TAB LAYOUT ───────────────────────────────────────────────────────────
    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName("browserTabs");
    m_tabs->setTabsClosable(true);
    mainLayout->addWidget(m_tabs, 1);

    // ── CONNECTIONS ──────────────────────────────────────────────────────────
    connect(m_addressBar, &QLineEdit::returnPressed, this, &VMBrowserOverlay::onNavigate);
    connect(m_searchBtn, &QPushButton::clicked, this, &VMBrowserOverlay::onNavigate);
    connect(m_newTabBtn, &QPushButton::clicked, this, &VMBrowserOverlay::onNewTab);
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &VMBrowserOverlay::onCloseTab);

    connect(m_backBtn, &QPushButton::clicked, this, [this]() {
        if (auto* b = qobject_cast<QTextBrowser*>(m_tabs->currentWidget())) b->backward();
    });
    connect(m_forwardBtn, &QPushButton::clicked, this, [this]() {
        if (auto* b = qobject_cast<QTextBrowser*>(m_tabs->currentWidget())) b->forward();
    });
    connect(m_refreshBtn, &QPushButton::clicked, this, [this]() {
        if (auto* b = qobject_cast<QTextBrowser*>(m_tabs->currentWidget())) b->reload();
    });

    connect(m_capFullBtn, &QPushButton::clicked, this, [this]() { handleCapture(0); });
    connect(m_capWinBtn, &QPushButton::clicked, this, [this]() { handleCapture(1); });
    connect(m_capRegBtn, &QPushButton::clicked, this, [this]() { handleCapture(2); });

    connect(m_nam, &QNetworkAccessManager::finished, this, &VMBrowserOverlay::onNetworkReply);
}

void VMBrowserOverlay::onNewTab() {
    QTextBrowser* browser = new QTextBrowser(m_tabs);
    browser->setObjectName("tabBrowser");
    browser->setOpenExternalLinks(false);

    int idx = m_tabs->addTab(browser, "New Tab");
    m_tabs->setCurrentIndex(idx);

    // Default welcome/search page template
    QString welcomeHtml = R"(
        <html>
        <body style="background-color: #0b0f19; color: #8b9bb4; font-family: 'Segoe UI', sans-serif; padding: 30px; text-align: center;">
            <div style="margin-top: 50px;">
                <span style="color: #00e5ff; font-size: 40px; font-weight: bold;">Google</span>
            </div>
            <div style="margin-top: 30px;">
                <p style="font-size: 14px; color: #64748b;">Integrated secure VM sandboxed browser overlay</p>
                <p style="font-size: 12px; color: #475569;">Type a search query or URL above to begin.</p>
            </div>
        </body>
        </html>
    )";
    browser->setHtml(welcomeHtml);
}

void VMBrowserOverlay::onCloseTab(int index) {
    if (m_tabs->count() > 1) {
        QWidget* w = m_tabs->widget(index);
        m_tabs->removeTab(index);
        w->deleteLater();
    }
}

void VMBrowserOverlay::onNavigate() {
    QString target = m_addressBar->text().trimmed();
    if (target.isEmpty()) return;

    QTextBrowser* activeBrowser = qobject_cast<QTextBrowser*>(m_tabs->currentWidget());
    if (!activeBrowser) return;

    if (target.contains(".") && !target.contains(" ")) {
        if (!target.startsWith("http://") && !target.startsWith("https://")) {
            target = "https://" + target;
        }
        loadUrl(target, activeBrowser);
    } else {
        // Search query
        m_tabs->setTabText(m_tabs->currentIndex(), QString("Search: %1").arg(target));
        
        // Generate simulated Google search result template with modern custom CSS styling
        QString searchResults = QString(R"(
            <html>
            <body style="background-color: #0b0f19; color: #d1d5db; font-family: 'Segoe UI', sans-serif; padding: 20px; line-height: 1.5;">
                <div style="border-bottom: 1px solid rgba(0, 229, 255, 0.2); padding-bottom: 15px; margin-bottom: 20px;">
                    <span style="color: #00e5ff; font-size: 22px; font-weight: bold;">Google Search Results</span>
                    <span style="color: #64748b; font-size: 12px; margin-left: 10px;">for: "%1"</span>
                </div>
                
                <div style="margin-bottom: 25px;">
                    <a href="https://leetcode.com" style="color: #00e5ff; font-size: 18px; font-weight: 600; text-decoration: none;">LeetCode Solutions & Optimal Algorithms</a>
                    <div style="color: #10b981; font-size: 12px;">https://leetcode.com/problems/solutions</div>
                    <p style="color: #9ca3af; font-size: 13px; margin: 4px 0 0 0;">Get optimized runtime scripts, coding cheat sheets, and dynamic programming walkthroughs for coding interviews.</p>
                </div>

                <div style="margin-bottom: 25px;">
                    <a href="https://github.com" style="color: #00e5ff; font-size: 18px; font-weight: 600; text-decoration: none;">GitHub Repository - Cheat Sheets</a>
                    <div style="color: #10b981; font-size: 12px;">https://github.com/developer-cheatsheets</div>
                    <p style="color: #9ca3af; font-size: 13px; margin: 4px 0 0 0;">An isolated playground of coding algorithms, syntax highlights, and API references matching interviewer expectations.</p>
                </div>

                <div style="margin-bottom: 25px;">
                    <a href="https://wikipedia.org" style="color: #00e5ff; font-size: 18px; font-weight: 600; text-decoration: none;">Wikipedia Reference Guides</a>
                    <div style="color: #10b981; font-size: 12px;">https://en.wikipedia.org/wiki/%1</div>
                    <p style="color: #9ca3af; font-size: 13px; margin: 4px 0 0 0;">Detailed explanation, complexity analysis, and mathematical formulations for standard computational procedures.</p>
                </div>
            </body>
            </html>
        )").arg(target);
        
        activeBrowser->setHtml(searchResults);
    }
}

void VMBrowserOverlay::loadUrl(const QString& url, QTextBrowser* browser) {
    m_tabs->setTabText(m_tabs->currentIndex(), "Loading...");
    
    QNetworkRequest req((QUrl(url)));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    QNetworkReply* reply = m_nam->get(req);
    m_activeRequests.insert(reply, browser);
}

void VMBrowserOverlay::onNetworkReply(QNetworkReply* reply) {
    QTextBrowser* browser = m_activeRequests.take(reply);
    if (!browser) {
        reply->deleteLater();
        return;
    }

    int tabIdx = m_tabs->indexOf(browser);

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QString html = QString::fromUtf8(data);
        
        // Basic display scaling of text content
        browser->setHtml(html);
        
        QUrl url = reply->url();
        m_tabs->setTabText(tabIdx, url.host());
        if (m_tabs->currentIndex() == tabIdx) {
            m_addressBar->setText(url.toString());
        }
    } else {
        QString errHtml = QString(R"(
            <html>
            <body style="background-color: #0b0f19; color: #ef4444; font-family: 'Segoe UI', sans-serif; padding: 30px; text-align: center;">
                <h3>Navigation Error</h3>
                <p style="color: #8b9bb4;">Failed to load requested page.</p>
                <p style="font-size: 11px; color: #6b7280;">Error details: %1</p>
            </body>
            </html>
        )").arg(reply->errorString());
        browser->setHtml(errHtml);
        m_tabs->setTabText(tabIdx, "Error");
    }

    reply->deleteLater();
}

void VMBrowserOverlay::handleCapture(int mode) {
    emit captureRequested(mode);
}

void VMBrowserOverlay::triggerGlobalShortcut() {
    // Slides in/out toggle triggers
    setVisible(!isVisible());
}

void VMBrowserOverlay::applyStyle() {
    setStyleSheet(R"(
        VMBrowserOverlay {
            background: rgba(10, 16, 32, 0.95);
            border-left: 1px solid rgba(0, 229, 255, 0.2);
        }
        
        /* NavBar styling */
        QWidget#navBar {
            background: rgba(4, 8, 16, 0.6);
            border: 1px solid rgba(0, 229, 255, 0.15);
            border-radius: 6px;
        }
        QPushButton#navBtn {
            background: transparent;
            border: 1px solid rgba(0, 229, 255, 0.1);
            border-radius: 4px;
            color: #8b9bb4;
            font-size: 11px;
            font-weight: bold;
        }
        QPushButton#navBtn:hover {
            background: rgba(0, 229, 255, 0.1);
            color: #00e5ff;
            border: 1px solid rgba(0, 229, 255, 0.3);
        }
        QLineEdit#addressBar {
            background: rgba(2, 4, 8, 0.8);
            border: 1px solid rgba(0, 229, 255, 0.15);
            border-radius: 4px;
            color: #e5e7eb;
            font-family: 'Segoe UI', sans-serif;
            font-size: 12px;
            padding: 0 8px;
        }
        QLineEdit#addressBar:focus {
            border: 1px solid rgba(0, 229, 255, 0.45);
        }
        QPushButton#searchBtn {
            background: rgba(0, 90, 110, 0.3);
            border: 1px solid rgba(0, 229, 255, 0.3);
            border-radius: 4px;
            color: #00e5ff;
            font-weight: bold;
            padding: 0 12px;
        }
        QPushButton#searchBtn:hover {
            background: rgba(0, 140, 170, 0.4);
            color: #ffffff;
        }

        /* Screenshot engine bar */
        QWidget#capBar {
            background: rgba(0, 229, 255, 0.04);
            border: 1px solid rgba(0, 229, 255, 0.1);
            border-radius: 6px;
        }
        QLabel#capLabel {
            color: #8b9bb4;
            font-family: 'Consolas', monospace;
            font-size: 10px;
            font-weight: bold;
            letter-spacing: 1px;
        }
        QPushButton#capBtn {
            background: rgba(14, 24, 44, 0.8);
            border: 1px solid rgba(0, 229, 255, 0.2);
            border-radius: 4px;
            color: #00e5ff;
            font-family: 'Segoe UI', sans-serif;
            font-size: 11px;
            font-weight: bold;
            padding: 0 10px;
        }
        QPushButton#capBtn:hover {
            background: rgba(0, 229, 255, 0.08);
            border: 1px solid rgba(0, 229, 255, 0.45);
            color: #ffffff;
        }

        /* Tabs styling */
        QTabWidget#browserTabs::pane {
            border: 1px solid rgba(0, 229, 255, 0.15);
            border-radius: 6px;
            background: rgba(4, 8, 16, 0.8);
        }
        QTabBar::tab {
            background: rgba(14, 24, 44, 0.5);
            border: 1px solid rgba(0, 229, 255, 0.1);
            border-bottom: none;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
            color: #8b9bb4;
            padding: 6px 12px;
            margin-right: 2px;
            font-size: 11px;
        }
        QTabBar::tab:hover {
            background: rgba(0, 229, 255, 0.05);
            color: #ffffff;
        }
        QTabBar::tab:selected {
            background: rgba(4, 8, 16, 0.8);
            border: 1px solid rgba(0, 229, 255, 0.2);
            border-bottom: none;
            color: #00e5ff;
            font-weight: bold;
        }
        
        QTextBrowser#tabBrowser {
            background: transparent;
            border: none;
            color: #d1d5db;
        }
    )");
}
