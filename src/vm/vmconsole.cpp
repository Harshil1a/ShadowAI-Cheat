#include "vmconsole.h"
#include "vmbrowser.h"
#include "vmscreenshot.h"
#include "vmsettings.h"
#include <QGraphicsDropShadowEffect>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QWindow>
#include <QMessageBox>
#include <QDebug>
#include <QDir>
#include <QGuiApplication>
#include <QClipboard>
#include <QSettings>
#include <QTimer>

VMConsoleWidget::VMConsoleWidget(QWidget* parent) : QWidget(parent) {
    m_screenshotEngine = new VMScreenshotEngine(this);

    setupUI();
    applyStyle();

    // Hook to VMManager state shifts
    connect(&VMManager::instance(), &VMManager::stateChanged, this, &VMConsoleWidget::onVmStateChanged);
    connect(&VMManager::instance(), &VMManager::sandboxWindowFound, this, &VMConsoleWidget::onSandboxWindowFound);
    connect(&VMManager::instance(), &VMManager::rdpSessionReady, this, &VMConsoleWidget::onRdpSessionReady);
    connect(&VMManager::instance(), &VMManager::logMessage, this, [this](const QString& msg) {
        qDebug() << "[VMManager]" << msg;
    });

    // Capture callbacks
    connect(m_browser, &VMBrowserOverlay::captureRequested, this, &VMConsoleWidget::handleCaptureRequest);
    connect(m_screenshotEngine, &VMScreenshotEngine::screenshotCaptured, this, &VMConsoleWidget::onScreenshotCaptured);
}

VMConsoleWidget::~VMConsoleWidget() {
    delete m_screenshotEngine;
}

void VMConsoleWidget::setupUI() {
    QHBoxLayout* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // Main workspace vertical layout (controls + viewport)
    QWidget* workspace = new QWidget(this);
    QVBoxLayout* wsLayout = new QVBoxLayout(workspace);
    wsLayout->setContentsMargins(16, 16, 16, 16);
    wsLayout->setSpacing(14);

    // ── CONTROL PANEL ────────────────────────────────────────────────────────
    m_controlPanel = new QWidget(workspace);
    m_controlPanel->setObjectName("controlPanel");
    m_controlPanel->setFixedHeight(50);
    
    QHBoxLayout* cpLayout = new QHBoxLayout(m_controlPanel);
    cpLayout->setContentsMargins(12, 0, 12, 0);
    cpLayout->setSpacing(10);

    m_launchBtn = new QPushButton("▶  LAUNCH VM", m_controlPanel);
    m_launchBtn->setObjectName("launchBtn");
    m_launchBtn->setFixedSize(130, 32);
    m_launchBtn->setCursor(Qt::PointingHandCursor);
    cpLayout->addWidget(m_launchBtn);

    m_stopBtn = new QPushButton("■  STOP VM", m_controlPanel);
    m_stopBtn->setObjectName("stopBtn");
    m_stopBtn->setFixedSize(110, 32);
    m_stopBtn->setCursor(Qt::PointingHandCursor);
    m_stopBtn->setEnabled(false);
    cpLayout->addWidget(m_stopBtn);

    m_settingsBtn = new QPushButton("⚙  VM SETTINGS", m_controlPanel);
    m_settingsBtn->setObjectName("settingsBtn");
    m_settingsBtn->setFixedSize(130, 32);
    m_settingsBtn->setCursor(Qt::PointingHandCursor);
    cpLayout->addWidget(m_settingsBtn);

    m_setupBtn = new QPushButton("⬡  RUN SETUP", m_controlPanel);
    m_setupBtn->setObjectName("setupBtn");
    m_setupBtn->setFixedSize(120, 32);
    m_setupBtn->setCursor(Qt::PointingHandCursor);
    cpLayout->addWidget(m_setupBtn);

    m_browserToggleBtn = new QPushButton("🌐 BROWSER OVERLAY", m_controlPanel);
    m_browserToggleBtn->setObjectName("browserToggleBtn");
    m_browserToggleBtn->setFixedSize(160, 32);
    m_browserToggleBtn->setCheckable(true);
    m_browserToggleBtn->setCursor(Qt::PointingHandCursor);
    cpLayout->addWidget(m_browserToggleBtn);

    cpLayout->addStretch();

    // Status label
    m_statusLabel = new QLabel("STATUS: STOPPED", m_controlPanel);
    m_statusLabel->setObjectName("statusLabel");
    cpLayout->addWidget(m_statusLabel);

    wsLayout->addWidget(m_controlPanel);

    // ── VIEWPORT CONTAINER ───────────────────────────────────────────────────
    m_viewportContainer = new QWidget(workspace);
    m_viewportContainer->setObjectName("viewportContainer");
    
    QVBoxLayout* vpLayout = new QVBoxLayout(m_viewportContainer);
    vpLayout->setContentsMargins(2, 2, 2, 2);
    
    m_placeholderLabel = new QLabel(
        "❖  VM SESSION OFFLINE  ❖\n\n"
        "The local RDP session is not running.\n"
        "Click \"LAUNCH VM\" above to start the isolated guest session.",
        m_viewportContainer
    );
    m_placeholderLabel->setObjectName("placeholderLabel");
    m_placeholderLabel->setAlignment(Qt::AlignCenter);
    vpLayout->addWidget(m_placeholderLabel);

    wsLayout->addWidget(m_viewportContainer, 1);
    rootLayout->addWidget(workspace, 1);

    // ── SLIDING BROWSER OVERLAY ──────────────────────────────────────────────
    m_browser = new VMBrowserOverlay(this);
    m_browser->setMaximumWidth(0); // Start closed
    m_browser->hide();
    rootLayout->addWidget(m_browser);

    // Connections
    connect(m_launchBtn,   &QPushButton::clicked, this, &VMConsoleWidget::onLaunchClicked);
    connect(m_stopBtn,     &QPushButton::clicked, this, &VMConsoleWidget::onStopClicked);
    connect(m_settingsBtn, &QPushButton::clicked, this, &VMConsoleWidget::onSettingsClicked);
    connect(m_setupBtn,    &QPushButton::clicked, this, &VMConsoleWidget::onSetupClicked);
    connect(m_browserToggleBtn, &QPushButton::clicked, this, &VMConsoleWidget::onToggleBrowser);
}

void VMConsoleWidget::onLaunchClicked() {
    m_launchBtn->setEnabled(false);
    VMManager::instance().startVM();
}

void VMConsoleWidget::onStopClicked() {
    m_stopBtn->setEnabled(false);
    
    QSettings settings("Microsoft", "RuntimeBroker");
    bool simulated = settings.value("vm/simulated", false).toBool();
    if (simulated) {
        onVmStateChanged(VMManager::Stopping);
        QTimer::singleShot(800, this, [this]() {
            onVmStateChanged(VMManager::Stopped);
        });
    } else {
        VMManager::instance().stopVM();
    }
}

void VMConsoleWidget::onSettingsClicked() {
    VMSettingsDialog dlg(this);
    dlg.exec();
}

void VMConsoleWidget::onSetupClicked() {
    // Stop any running VM first
    VMManager::instance().stopVM();

    QString appDir = QCoreApplication::applicationDirPath();
    QString scriptPath = QDir::toNativeSeparators(appDir + "/setup_shadowai_rdp.ps1");
    if (!QFile::exists(scriptPath)) {
        scriptPath = QDir::toNativeSeparators(QDir::currentPath() + "/build/bin/setup_shadowai_rdp.ps1");
    }
    if (!QFile::exists(scriptPath)) {
        scriptPath = QDir::toNativeSeparators(QDir::currentPath() + "/setup_shadowai_rdp.ps1");
    }

    QString ps = QString("Start-Process powershell.exe -ArgumentList '-NoProfile -ExecutionPolicy Bypass -File \"%1\"' -Verb RunAs").arg(scriptPath);
    QProcess::startDetached("powershell.exe", QStringList() << "-NoProfile" << "-ExecutionPolicy" << "Bypass" << "-Command" << ps);
}

void VMConsoleWidget::onVmStateChanged(VMManager::VMState newState) {
    switch (newState) {
        case VMManager::Stopped:
            m_statusLabel->setText("STATUS: STOPPED");
            m_statusLabel->setStyleSheet("color: #8b9bb4; font-weight: bold; font-family: 'Consolas';");
            m_launchBtn->setEnabled(true);
            m_stopBtn->setEnabled(false);
            
            // Clean up RDP widget
            if (m_rdpWidget) {
                m_rdpWidget->disconnectSession();
                m_rdpWidget->setParent(nullptr);
                m_rdpWidget->deleteLater();
                m_rdpWidget = nullptr;
            }
            // Clean up embedded view
            if (m_embeddedContainer) {
                m_embeddedContainer->setParent(nullptr);
                m_embeddedContainer->deleteLater();
                m_embeddedContainer = nullptr;
            }
            m_placeholderLabel->show();
            break;
            
        case VMManager::Starting:
            m_statusLabel->setText("STATUS: STARTING...");
            m_statusLabel->setStyleSheet("color: #f59e0b; font-weight: bold; font-family: 'Consolas';");
            m_launchBtn->setEnabled(false);
            m_stopBtn->setEnabled(true);
            break;
            
        case VMManager::Running:
            m_statusLabel->setText("STATUS: RUNNING");
            m_statusLabel->setStyleSheet("color: #10b981; font-weight: bold; font-family: 'Consolas';");
            m_launchBtn->setEnabled(false);
            m_stopBtn->setEnabled(true);
            break;
            
        case VMManager::Stopping:
            m_statusLabel->setText("STATUS: STOPPING...");
            m_statusLabel->setStyleSheet("color: #f59e0b; font-weight: bold; font-family: 'Consolas';");
            m_launchBtn->setEnabled(false);
            m_stopBtn->setEnabled(false);
            break;
            
        case VMManager::Error:
            m_statusLabel->setText("STATUS: ERROR");
            m_statusLabel->setStyleSheet("color: #ef4444; font-weight: bold; font-family: 'Consolas';");
            m_launchBtn->setEnabled(true);
            m_stopBtn->setEnabled(false);
            QMessageBox::warning(this, "VM Launch Failure", 
                "Windows Sandbox failed to initialize.\nPlease verify that Hyper-V and Windows Sandbox features are enabled in Windows.");
            break;
    }
}

void VMConsoleWidget::onSandboxWindowFound(void* hwnd) {
#ifdef Q_OS_WIN
    HWND winId = (HWND)hwnd;
    QWindow* window = QWindow::fromWinId((WId)winId);
    if (!window) return;

    if (m_embeddedContainer) {
        m_embeddedContainer->setParent(nullptr);
        m_embeddedContainer->deleteLater();
    }

    m_embeddedContainer = QWidget::createWindowContainer(window, m_viewportContainer);
    
    QVBoxLayout* l = qobject_cast<QVBoxLayout*>(m_viewportContainer->layout());
    if (l) {
        m_placeholderLabel->hide();
        l->addWidget(m_embeddedContainer);
    }
#else
    Q_UNUSED(hwnd)
#endif
}

void VMConsoleWidget::onRdpSessionReady(const QString& username) {
    // Clean up any existing embedded view
    if (m_rdpWidget) {
        m_rdpWidget->setParent(nullptr);
        m_rdpWidget->deleteLater();
        m_rdpWidget = nullptr;
    }
    if (m_embeddedContainer) {
        m_embeddedContainer->setParent(nullptr);
        m_embeddedContainer->deleteLater();
        m_embeddedContainer = nullptr;
    }

    // Create and show the RDP widget
    m_rdpWidget = new VMRdpWidget(m_viewportContainer);
    m_rdpWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QVBoxLayout* l = qobject_cast<QVBoxLayout*>(m_viewportContainer->layout());
    if (l) {
        m_placeholderLabel->hide();
        l->addWidget(m_rdpWidget);
    }

    connect(m_rdpWidget, &VMRdpWidget::connected, this, [this]() {
        m_statusLabel->setText("STATUS: RUNNING (RDP)");
        m_statusLabel->setStyleSheet("color: #10b981; font-weight: bold; font-family: 'Consolas';");
    });
    connect(m_rdpWidget, &VMRdpWidget::connectionFailed, this, [this](const QString& reason) {
        m_statusLabel->setText("STATUS: ERROR");
        m_statusLabel->setStyleSheet("color: #ef4444; font-weight: bold; font-family: 'Consolas';");
        QMessageBox::warning(this, "RDP Connection Failed",
            QString("Could not connect to local RDP session.\n\nReason: %1\n\n"
                    "Make sure you ran the Setup first (as Administrator).").arg(reason));
        m_launchBtn->setEnabled(true);
        m_stopBtn->setEnabled(false);
    });

    m_rdpWidget->connectToLocalSession(username);
}

void VMConsoleWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (m_browserOpen && m_browser) {
        m_browser->setMaximumWidth(400);
    }
}

void VMConsoleWidget::onToggleBrowser() {
    m_browserOpen = !m_browserOpen;
    m_browserToggleBtn->setChecked(m_browserOpen);
    animateBrowser(m_browserOpen);
}

void VMConsoleWidget::animateBrowser(bool open) {
    QPropertyAnimation* animation = new QPropertyAnimation(m_browser, "maximumWidth", this);
    animation->setDuration(250);
    
    if (open) {
        m_browser->show();
        animation->setStartValue(0);
        animation->setEndValue(400);
    } else {
        animation->setStartValue(400);
        animation->setEndValue(0);
        connect(animation, &QPropertyAnimation::finished, m_browser, &QWidget::hide);
    }
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void VMConsoleWidget::handleCaptureRequest(int mode) {
    HWND sandboxHwnd = VMManager::instance().getSandboxWindow();
    
    if (mode == 0) {
        // Full screen capture
        QPixmap shot = m_screenshotEngine->captureFull(sandboxHwnd);
        onScreenshotCaptured(shot);
    } else if (mode == 1) {
        // Active window capture (the sandbox window)
        QPixmap shot = m_screenshotEngine->captureActiveWindow(sandboxHwnd);
        onScreenshotCaptured(shot);
    } else if (mode == 2) {
        // Region select
        // Ensure selection is drawn over viewport container
        m_screenshotEngine->startRegionCapture(m_viewportContainer, sandboxHwnd);
    }
}

void VMConsoleWidget::onScreenshotCaptured(const QPixmap& pixmap) {
    if (pixmap.isNull()) return;

    // Save screenshot to VM shared folder so guest tools or local AI can access it
    QString sharedDir = VMManager::instance().getSharedFolderPath();
    QString path = QDir(sharedDir).filePath("vm_screenshot.png");
    pixmap.save(path, "PNG");

    // Copy to clipboard dynamically if enabled
    QGuiApplication::clipboard()->setPixmap(pixmap);

    QMessageBox::information(this, "Screenshot Captured", 
        QString("Screenshot captured successfully!\nSaved to Shared Folder: %1").arg(path));
}

void VMConsoleWidget::applyStyle() {
    setStyleSheet(R"(
        VMConsoleWidget {
            background: transparent;
        }
        
        /* Control Panel */
        QWidget#controlPanel {
            background: rgba(14, 24, 44, 0.6);
            border: 1px solid rgba(0, 229, 255, 0.15);
            border-radius: 8px;
        }
        QPushButton#launchBtn {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #0d3a2a, stop:0.45 #0a3020, stop:1 #082818);
            border-top: 2px solid rgba(16, 185, 129, 0.7);
            border-left: 2px solid rgba(16, 185, 129, 0.45);
            border-radius: 6px;
            color: #10b981;
            font-size: 11px;
            font-weight: bold;
            letter-spacing: 1.5px;
        }
        QPushButton#launchBtn:hover:enabled {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #144838, stop:1 #0b3024);
            color: #ffffff;
            border-top: 2px solid rgba(16, 185, 129, 0.9);
        }
        QPushButton#stopBtn {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #3a0d0d, stop:0.45 #300a0a, stop:1 #280808);
            border-top: 2px solid rgba(239, 68, 68, 0.7);
            border-left: 2px solid rgba(239, 68, 68, 0.45);
            border-radius: 6px;
            color: #ef4444;
            font-size: 11px;
            font-weight: bold;
            letter-spacing: 1.5px;
        }
        QPushButton#stopBtn:hover:enabled {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #481414, stop:1 #300c0c);
            color: #ffffff;
            border-top: 2px solid rgba(239, 68, 68, 0.9);
        }
        QPushButton#settingsBtn {
            background: rgba(14, 24, 44, 0.8);
            border: 1px solid rgba(0, 229, 255, 0.2);
            border-radius: 6px;
            color: #8b9bb4;
            font-size: 11px;
            font-weight: bold;
            letter-spacing: 1px;
        }
        QPushButton#settingsBtn:hover:enabled {
            color: #00e5ff;
            background: rgba(0, 229, 255, 0.08);
            border: 1px solid rgba(0, 229, 255, 0.4);
        }
        QPushButton#setupBtn {
            background: rgba(14, 24, 44, 0.8);
            border: 1px solid rgba(251, 191, 36, 0.3);
            border-radius: 6px;
            color: #fbbf24;
            font-size: 11px;
            font-weight: bold;
            letter-spacing: 1px;
        }
        QPushButton#setupBtn:hover:enabled {
            color: #ffffff;
            background: rgba(251, 191, 36, 0.1);
            border: 1px solid rgba(251, 191, 36, 0.6);
        }
        QPushButton#browserToggleBtn {
            background: rgba(14, 24, 44, 0.8);
            border: 1px solid rgba(0, 229, 255, 0.2);
            border-radius: 6px;
            color: #8b9bb4;
            font-size: 11px;
            font-weight: bold;
            letter-spacing: 1px;
        }
        QPushButton#browserToggleBtn:hover {
            color: #00e5ff;
            background: rgba(0, 229, 255, 0.08);
            border: 1px solid rgba(0, 229, 255, 0.4);
        }
        QPushButton#browserToggleBtn:checked {
            background: rgba(0, 229, 255, 0.12);
            border: 1px solid #00e5ff;
            color: #00e5ff;
        }

        QLabel#statusLabel {
            font-size: 11px;
            margin-left: 6px;
            color: #8b9bb4;
            font-family: 'Consolas', monospace;
            font-weight: bold;
        }

        /* Viewport */
        QWidget#viewportContainer {
            background: rgba(4, 8, 18, 0.85);
            border: 1px solid rgba(0, 229, 255, 0.15);
            border-radius: 8px;
        }
        QLabel#placeholderLabel {
            color: #64748b;
            font-size: 13px;
            font-family: 'Segoe UI', sans-serif;
            font-weight: 500;
            line-height: 1.6;
        }
    )");
}
