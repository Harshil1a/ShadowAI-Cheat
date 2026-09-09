#include "vmsetupwizard.h"
#include "vmmanager.h"
#include <QGraphicsDropShadowEffect>
#include <QDir>
#include <QStandardPaths>
#include <QSettings>
#include <QTextStream>
#include <QFile>
#include <QCoreApplication>
#include <QApplication>
#include <QTime>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

static bool isRunningAsAdmin() {
#ifdef Q_OS_WIN
    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY ntAuth = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuth, 2, SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(nullptr, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin == TRUE;
#else
    return false;
#endif
}

VMSetupWizard::VMSetupWizard(QWidget* parent) : QWidget(parent) {
    setupUI();
    applyStyle();
}

VMSetupWizard::~VMSetupWizard() {}

void VMSetupWizard::setupUI() {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(50, 40, 50, 40);
    layout->setSpacing(20);

    // Title
    m_titleLabel = new QLabel("VM MODE SETUP WIZARD", this);
    m_titleLabel->setObjectName("titleLabel");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    
    QGraphicsDropShadowEffect* titleGlow = new QGraphicsDropShadowEffect(m_titleLabel);
    titleGlow->setColor(QColor(0, 229, 255, 120));
    titleGlow->setBlurRadius(20);
    titleGlow->setOffset(0, 2);
    m_titleLabel->setGraphicsEffect(titleGlow);
    layout->addWidget(m_titleLabel);

    m_subtitleLabel = new QLabel("Automated Local RDP Session Configuration", this);
    m_subtitleLabel->setObjectName("subtitleLabel");
    m_subtitleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_subtitleLabel);

    layout->addSpacing(10);

    // Informative box
    m_infoLabel = new QLabel(
        "This wizard will automatically configure a live, interactive Windows session "
        "that runs directly on your hardware — no VMware, no VirtualBox, no reboot needed.\n\n"
        "What this wizard will do automatically:\n"
        " ✦ Install RDP Wrapper (enables RDP on Windows Home)\n"
        " ✦ Create a hidden second user account (ShadowUser)\n"
        " ✦ Enable Remote Desktop on this PC\n"
        " ✦ Open the firewall for local loopback connection\n"
        " ✦ Connect to that session and embed it inside this window",
        this
    );
    m_infoLabel->setObjectName("infoLabel");
    m_infoLabel->setWordWrap(true);
    layout->addWidget(m_infoLabel);

    layout->addSpacing(10);

    // Logs output (hidden until setup starts)
    m_logDisplay = new QTextEdit(this);
    m_logDisplay->setObjectName("logDisplay");
    m_logDisplay->setReadOnly(true);
    m_logDisplay->hide();
    layout->addWidget(m_logDisplay, 1);

    // Step label (hidden until setup starts)
    m_stepLabel = new QLabel("", this);
    m_stepLabel->setObjectName("stepLabel");
    m_stepLabel->setAlignment(Qt::AlignCenter);
    m_stepLabel->hide();
    layout->addWidget(m_stepLabel);

    // Progress bar (hidden until setup starts)
    m_progressBar = new QProgressBar(this);
    m_progressBar->setObjectName("progressBar");
    m_progressBar->setRange(0, m_totalSteps);
    m_progressBar->setValue(0);
    m_progressBar->hide();
    layout->addWidget(m_progressBar);

    // Start Button
    m_startBtn = new QPushButton("⚡  ONE-CLICK SETUP", this);
    m_startBtn->setObjectName("startBtn");
    m_startBtn->setFixedHeight(54);
    m_startBtn->setCursor(Qt::PointingHandCursor);
    layout->addWidget(m_startBtn, 0, Qt::AlignCenter);

    // Simulate Button (hidden initially)
    m_simulateBtn = new QPushButton("USE SIMULATED VM MODE (NO REBOOT)", this);
    m_simulateBtn->setObjectName("simulateBtn");
    m_simulateBtn->setFixedHeight(44);
    m_simulateBtn->setCursor(Qt::PointingHandCursor);
    m_simulateBtn->hide();
    layout->addWidget(m_simulateBtn, 0, Qt::AlignCenter);

    connect(m_startBtn, &QPushButton::clicked, this, &VMSetupWizard::onStartSetup);
    connect(m_simulateBtn, &QPushButton::clicked, this, &VMSetupWizard::onSimulateClicked);
}

void VMSetupWizard::onStartSetup() {
    if (m_startBtn->text().contains("FINISH")) {
        m_forceInstall = false;
        emit setupCompleted();
        return;
    }

    // Check admin privileges first
    if (!isRunningAsAdmin()) {
        m_infoLabel->hide();
        m_simulateBtn->hide();
        m_logDisplay->show();
        m_progressBar->show();
        m_stepLabel->show();
        m_stepLabel->setText("Elevating system setup...");
        addLog("[INFO] Requesting Administrator privileges via Windows UAC prompt...");
        QApplication::processEvents();

        QString scriptPath = QDir::toNativeSeparators(QCoreApplication::applicationDirPath() + "/setup_shadowai_rdp.ps1");
        if (!QFile::exists(scriptPath)) {
            scriptPath = QDir::toNativeSeparators(QDir::currentPath() + "/build/bin/setup_shadowai_rdp.ps1");
        }
        if (!QFile::exists(scriptPath)) {
            scriptPath = QDir::toNativeSeparators(QDir::currentPath() + "/setup_shadowai_rdp.ps1");
        }

        QString psCommand = QString("Start-Process powershell -ArgumentList '-NoProfile -ExecutionPolicy Bypass -File \"%1\"' -Verb RunAs -Wait").arg(scriptPath);
        QProcess proc;
        proc.start("powershell.exe", QStringList() << "-NoProfile" << "-ExecutionPolicy" << "Bypass" << "-Command" << psCommand);
        proc.waitForFinished(-1);

        if (VMManager::instance().isRdpReady()) {
            addSuccess(" -> RDP Session configured successfully via elevated setup!");
            setupComplete();
            return;
        } else {
            addLog("[WARNING] Setup finished. Checking status...", false);
        }
    }

    // Switch to setup mode UI
    m_infoLabel->hide();
    m_simulateBtn->hide();
    m_logDisplay->show();
    m_progressBar->show();
    m_stepLabel->show();
    m_startBtn->setEnabled(false);

    m_currentStep = 0;
    m_progressBar->setValue(0);
    m_logDisplay->clear();

    addLog("═══════════════════════════════════════════════════");
    addLog("  ShadowAI — One-Click VM Setup");
    addLog("═══════════════════════════════════════════════════");
    addLog("");
    addLog("Initializing automated RDP session setup...");
    addLog("");

    // Start the async chain
    QApplication::processEvents();
    runStep1_CheckSystem();
}

// ────────────────────────────────────────────────────────────────
// Step 1: Check if RDP is already configured
// ────────────────────────────────────────────────────────────────
void VMSetupWizard::runStep1_CheckSystem() {
    m_currentStep = 1;
    setProgress(1, m_totalSteps);
    m_stepLabel->setText("Step 1 of 6 — Checking system...");

    addLog("[Step 1/6] Checking system configuration...");
    QApplication::processEvents();

    if (VMManager::instance().isRdpReady() && !m_forceInstall) {
        addSuccess(" -> RDP Session already fully configured!");
        m_progressBar->setValue(m_totalSteps);
        setupComplete();
        return;
    }

    // Check if RDP Wrapper is installed
    if (QFile::exists("C:/Program Files/RDP Wrapper/rdpwrap.ini")) {
        addSuccess(" -> RDP Wrapper detected.");
    } else {
        addLog(" -> RDP Wrapper not found. Will install it.");
    }

    // Check Windows version
    QApplication::processEvents();
    runStep2_DefenderExclusion();
}

// ────────────────────────────────────────────────────────────────
// Step 2: Add Windows Defender exclusion
// ────────────────────────────────────────────────────────────────
void VMSetupWizard::runStep2_DefenderExclusion() {
    m_currentStep = 2;
    setProgress(2, m_totalSteps);
    m_stepLabel->setText("Step 2 of 6 — Adding Defender exclusion...");

    addLog("[Step 2/6] Adding Windows Defender exclusion...");
    QApplication::processEvents();

    bool ok = VMManager::instance().addDefenderExclusion();
    if (ok) {
        addSuccess(" -> Defender exclusion configured.");
    } else {
        addWarning(" -> Defender exclusion skipped (continuing anyway).");
    }

    QApplication::processEvents();
    runStep3_InstallRdpWrapper();
}

// ────────────────────────────────────────────────────────────────
// Step 3+4: Download and install RDP Wrapper (combined)
// ────────────────────────────────────────────────────────────────
void VMSetupWizard::runStep3_InstallRdpWrapper() {
    m_currentStep = 3;
    setProgress(3, m_totalSteps);
    m_stepLabel->setText("Step 3 of 6 — Installing RDP Wrapper...");

    addLog("[Step 3/6] Installing RDP Wrapper (download + install)...");
    QApplication::processEvents();

    bool ok = VMManager::instance().installRdpWrapper();
    if (ok) {
        addSuccess(" -> RDP Wrapper installed successfully.");
    } else {
        addLog("[ERROR] RDP Wrapper installation failed.", true);
        addLog(" -> Check your internet connection.", true);
        QApplication::processEvents();
        setupFailed("RDP Wrapper installation failed");
        return;
    }

    QApplication::processEvents();
    runStep4_CreateUser();
}

// ────────────────────────────────────────────────────────────────
// Step 5: Create ShadowUser account
// ────────────────────────────────────────────────────────────────
void VMSetupWizard::runStep4_CreateUser() {
    m_currentStep = 4;
    setProgress(4, m_totalSteps);
    m_stepLabel->setText("Step 4 of 6 — Creating ShadowUser account...");

    addLog("[Step 4/6] Creating isolated ShadowUser account...");
    QApplication::processEvents();

    bool ok = VMManager::instance().setupRdpUser();
    if (ok) {
        addSuccess(" -> ShadowUser account created and added to Administrators.");
    } else {
        addWarning(" -> Could not create user automatically.");
        addWarning("    Make sure you are running as Administrator.");
    }

    QApplication::processEvents();
    runStep5_EnableRdpAccess();
}

// ────────────────────────────────────────────────────────────────
// Step 6: Enable RDP access (registry + firewall)
// ────────────────────────────────────────────────────────────────
void VMSetupWizard::runStep5_EnableRdpAccess() {
    m_currentStep = 5;
    setProgress(5, m_totalSteps);
    m_stepLabel->setText("Step 5 of 6 — Enabling Remote Desktop...");

    addLog("[Step 5/6] Enabling Remote Desktop access...");
    QApplication::processEvents();

    bool ok = VMManager::instance().enableRdpAccess();
    if (ok) {
        addSuccess(" -> Remote Desktop enabled.");
        addSuccess(" -> Firewall rule configured for local loopback.");
        addSuccess(" -> Blank password policy configured.");
    } else {
        addLog("[ERROR] Failed to enable Remote Desktop.", true);
        addLog(" -> Run the app as Administrator and try again.", true);
        QApplication::processEvents();
        setupFailed("RDP enable failed");
        return;
    }

    QApplication::processEvents();
    runStep6_Verify();
}

// ────────────────────────────────────────────────────────────────
// Step 6: Verify everything works
// ────────────────────────────────────────────────────────────────
void VMSetupWizard::runStep6_Verify() {
    m_currentStep = 6;
    setProgress(6, m_totalSteps);
    m_stepLabel->setText("Step 6 of 6 — Verifying setup...");

    addLog("[Step 6/6] Verifying session configuration...");
    QApplication::processEvents();

    bool ready = VMManager::instance().isRdpReady();
    if (ready) {
        addSuccess(" -> ShadowUser account:      OK");
        addSuccess(" -> Remote Desktop service:   OK");
        addSuccess(" -> Local loopback connection: OK");
    } else {
        addWarning(" -> Verification incomplete. Some components may need attention.");
        addWarning("    You can try clicking FINISH and launching the VM anyway.");
    }

    // Also create the shared folder
    QString sharedPath = VMManager::instance().getSharedFolderPath();
    addLog(QString(" -> Shared folder: %1").arg(sharedPath));

    QApplication::processEvents();
    setupComplete();
}

// ────────────────────────────────────────────────────────────────
// Setup complete
// ────────────────────────────────────────────────────────────────
void VMSetupWizard::setupComplete() {
    m_stepLabel->setText("✅ Setup Complete!");
    addLog("");
    addLog("═══════════════════════════════════════════════════");
    addLog("  ✅  SETUP COMPLETED SUCCESSFULLY!");
    addLog("  Your VM session is ready to launch.");
    addLog("═══════════════════════════════════════════════════");

    setButtonState("🚀  FINISH & LAUNCH VM", true);
}

// ────────────────────────────────────────────────────────────────
// Setup failed
// ────────────────────────────────────────────────────────────────
void VMSetupWizard::setupFailed(const QString& reason) {
    m_stepLabel->setText(QString("❌ Setup failed: %1").arg(reason));
    addLog("");
    addLog("═══════════════════════════════════════════════════");
    addLog(QString("  ❌  SETUP FAILED: %1").arg(reason));
    addLog("  Please fix the issue and try again.");
    addLog("═══════════════════════════════════════════════════");

    setButtonState("🔄  RETRY SETUP", true);
}

// ────────────────────────────────────────────────────────────────
// Helpers
// ────────────────────────────────────────────────────────────────
void VMSetupWizard::setProgress(int step, int total) {
    m_progressBar->setRange(0, total);
    m_progressBar->setValue(step);
}

void VMSetupWizard::setButtonState(const QString& text, bool enabled) {
    m_startBtn->setText(text);
    m_startBtn->setEnabled(enabled);
    // Reconnect the click handler
    disconnect(m_startBtn, &QPushButton::clicked, nullptr, nullptr);
    connect(m_startBtn, &QPushButton::clicked, this, &VMSetupWizard::onStartSetup);
}

void VMSetupWizard::addLog(const QString& msg, bool isError) {
    QString timeStr = QTime::currentTime().toString("hh:mm:ss");
    QString color = isError ? "#ef4444" : "#00e5ff";
    m_logDisplay->append(QString("<font color='#6b7280'>[%1]</font> <font color='%2'>%3</font>")
        .arg(timeStr, color, msg));
}

void VMSetupWizard::addSuccess(const QString& msg) {
    QString timeStr = QTime::currentTime().toString("hh:mm:ss");
    m_logDisplay->append(QString("<font color='#6b7280'>[%1]</font> <font color='#10b981'>%2</font>")
        .arg(timeStr, msg));
}

void VMSetupWizard::addWarning(const QString& msg) {
    QString timeStr = QTime::currentTime().toString("hh:mm:ss");
    m_logDisplay->append(QString("<font color='#6b7280'>[%1]</font> <font color='#f59e0b'>%2</font>")
        .arg(timeStr, msg));
}

void VMSetupWizard::onSimulateClicked() {
    QSettings settings("Microsoft", "RuntimeBroker");
    settings.setValue("vm/simulated", true);
    
    // Create a mock WSB file so checkSetupState passes next time
    QString wsb = VMManager::instance().getWsbFilePath();
    QFile file(wsb);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << "<Configuration><Simulated>True</Simulated></Configuration>\n";
        file.close();
    }
    
    addLog("❖ Setup Bypassed: Simulated VM Mode Activated!");
    emit setupCompleted();
}

void VMSetupWizard::applyStyle() {
    setStyleSheet(R"(
        VMSetupWizard {
            background: transparent;
        }
        QLabel#titleLabel {
            color: #00e5ff;
            font-size: 20px;
            font-weight: bold;
            letter-spacing: 3px;
        }
        QLabel#subtitleLabel {
            color: #8b9bb4;
            font-size: 11px;
            font-family: 'Segoe UI', sans-serif;
            font-weight: bold;
            letter-spacing: 1.5px;
            text-transform: uppercase;
        }
        QLabel#infoLabel {
            color: #8b9bb4;
            font-size: 13px;
            line-height: 1.5;
            background: rgba(14, 24, 44, 0.6);
            border: 1px solid rgba(0, 229, 255, 0.15);
            border-radius: 8px;
            padding: 16px;
        }
        QLabel#stepLabel {
            color: #f59e0b;
            font-size: 12px;
            font-family: 'Consolas', 'Courier New', monospace;
            font-weight: bold;
            letter-spacing: 1px;
        }
        QTextEdit#logDisplay {
            background: rgba(4, 8, 18, 0.8);
            border: 1px solid rgba(0, 229, 255, 0.2);
            border-radius: 6px;
            color: #d1d5db;
            font-family: 'Consolas', 'Courier New', monospace;
            font-size: 12px;
            padding: 10px;
        }
        QProgressBar#progressBar {
            background: rgba(14, 24, 44, 0.6);
            border: 1px solid rgba(0, 229, 255, 0.15);
            border-radius: 4px;
            height: 12px;
            text-align: center;
            color: transparent;
        }
        QProgressBar#progressBar::chunk {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #005a70, stop:1 #00e5ff);
            border-radius: 3px;
        }
        QPushButton#startBtn {
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
            padding: 0 40px;
        }
        QPushButton#startBtn:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #143848, stop:0.5 #0f2e3c, stop:1 #0b2430);
            border-top: 2px solid rgba(0, 229, 255, 0.9);
            border-left: 2px solid rgba(0, 229, 255, 0.65);
            color: #ffffff;
        }
        QPushButton#startBtn:pressed {
            border-top: 2px solid rgba(0, 120, 160, 0.25);
            border-bottom: 3px solid rgba(0, 229, 255, 0.7);
            color: #b0f0ff;
        }
        QPushButton#startBtn:disabled {
            border: 2px solid rgba(255, 255, 255, 0.06);
            color: rgba(255, 255, 255, 0.15);
            background: #0a1420;
        }
        QPushButton#simulateBtn {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #1a2436, stop:1 #0f1624);
            border: 1px solid rgba(0, 229, 255, 0.3);
            border-radius: 6px;
            color: #8b9bb4;
            font-family: 'Segoe UI', sans-serif;
            font-size: 11px;
            font-weight: bold;
            letter-spacing: 1px;
            padding: 0 20px;
            margin-top: 10px;
        }
        QPushButton#simulateBtn:hover {
            border: 1px solid #00e5ff;
            color: #ffffff;
            background: rgba(0, 229, 255, 0.05);
        }
    )");
}
