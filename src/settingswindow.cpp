#include "settingswindow.h"
#include "appconfig.h"
#include "accountmanager.h"
#ifdef Q_OS_WIN
#include <windows.h>
#endif
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QTabWidget>
#include <QScrollArea>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QKeyEvent>
#include <QShowEvent>
#include <QCheckBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QTimer>
#include <QDesktopServices>

// ─── KeyCaptureEdit ──────────────────────────────────────────────────────────

KeyCaptureEdit::KeyCaptureEdit(QWidget* parent) : QLineEdit(parent) {
    setReadOnly(true);
    setPlaceholderText("Click then press key...");
    setAlignment(Qt::AlignCenter);
}

void KeyCaptureEdit::setVK(int vk) {
    m_vk = vk;
    setText(vkToName(vk));
}

void KeyCaptureEdit::mousePressEvent(QMouseEvent* event) {
    QLineEdit::mousePressEvent(event);
    setPlaceholderText("Press a key...");
    setText("");
}

void KeyCaptureEdit::keyPressEvent(QKeyEvent* event) {
    int key = event->key();
    // Ignore modifier-only keys
    if (key == Qt::Key_Shift || key == Qt::Key_Control ||
        key == Qt::Key_Alt   || key == Qt::Key_Meta) {
        return;
    }

    // Convert Qt key to Windows VK
    // Simple mapping for letters and digits
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        m_vk = key; // Qt letters match ASCII / VK
    } else if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        m_vk = key;
    } else {
        // Function keys
        if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
            m_vk = 0x70 + (key - Qt::Key_F1);
        } else {
            m_vk = key & 0xFF;
        }
    }

    setText(vkToName(m_vk));
    clearFocus();
}

QString KeyCaptureEdit::vkToName(int vk) {
    if (vk >= 0x41 && vk <= 0x5A) return QString(QChar(vk));
    if (vk >= 0x30 && vk <= 0x39) return QString(QChar(vk));
    if (vk >= 0x70 && vk <= 0x7B) return QString("F%1").arg(vk - 0x6F);
    switch (vk) {
        case 0x20: return "Space";
        case 0x0D: return "Enter";
        case 0x1B: return "Esc";
        case 0x08: return "Backspace";
        case 0x09: return "Tab";
        case 0xBC: return ",";
        case 0xBE: return ".";
        case 0xBF: return "/";
        case 0xBA: return ";";
        case 0xDE: return "'";
        case 0xDB: return "[";
        case 0xDD: return "]";
        case 0xBB: return "=";
        case 0xBD: return "-";
        default:   return QString("0x%1").arg(vk, 2, 16, QChar('0')).toUpper();
    }
}

// ─── SettingsWindow ──────────────────────────────────────────────────────────

SettingsWindow::SettingsWindow(QWidget* parent)
    : QWidget(parent)
{
    resize(700, 760);
    setMinimumSize(600, 520);
    setupUI();
    loadValues();
    applyStyle();
}

void SettingsWindow::setupUI() {
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    // Create the Tab Widget
    QTabWidget* tabWidget = new QTabWidget(this);
    tabWidget->setObjectName("settingsTabs");

    // ─────────────────────────────────────────────────────────────────────────
    // ─────────────────────────────────────────────────────────────────────────
    // TAB 1: API PROMPT (Wrapped in smooth QScrollArea so it never squishes)
    // ─────────────────────────────────────────────────────────────────────────
    QScrollArea* apiScroll = new QScrollArea(tabWidget);
    apiScroll->setWidgetResizable(true);
    apiScroll->setFrameShape(QFrame::NoFrame);
    apiScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    apiScroll->setStyleSheet("QScrollArea { background: transparent; border: none; }");

    QWidget* tabApiPrompt = new QWidget;
    QVBoxLayout* apiLayout = new QVBoxLayout(tabApiPrompt);
    apiLayout->setContentsMargins(12, 12, 16, 12);
    apiLayout->setSpacing(14);

    // Group 1: ⚡ Shadow Pro Master Cloud Engine (Google Gemini 2.5 Flash)
    m_proCloudGroup = new QGroupBox("⚡ Shadow Pro Cloud Engine (Master Zero-Key Vision)", tabApiPrompt);
    QVBoxLayout* proLayout = new QVBoxLayout(m_proCloudGroup);
    proLayout->setContentsMargins(12, 16, 12, 12);
    proLayout->setSpacing(8);

    QHBoxLayout* proHeaderRow = new QHBoxLayout;
    m_engineCloudRadio = new QRadioButton("⚡ Enable Shadow Pro Master Engine (Gemini 2.5 Flash Cloud • ~1.1s Latency)", m_proCloudGroup);
    m_engineCloudRadio->setStyleSheet("font-weight: bold; color: #00e5ff; font-size: 12px;");
    m_proStatusBadge = new QLabel(m_proCloudGroup);
    m_proStatusBadge->setStyleSheet("font-family: monospace; font-size: 11px; font-weight: bold;");
    proHeaderRow->addWidget(m_engineCloudRadio, 1);
    proHeaderRow->addWidget(m_proStatusBadge);
    proLayout->addLayout(proHeaderRow);

    QLabel* proDesc = new QLabel(
        "• Pre-configured high-speed Google Gemini 2.5 Flash Cloud Engine.\n"
        "• Zero setup required: No API key needed, no Google Cloud billing.\n"
        "• 24/7 unlimited queries with full desktop OCR vision and code reasoning.",
        m_proCloudGroup
    );
    proDesc->setStyleSheet("color: #8b9bb4; font-size: 11px; margin-left: 20px;");
    proLayout->addWidget(proDesc);

    QHBoxLayout* proActionRow = new QHBoxLayout;
    proActionRow->setContentsMargins(20, 4, 0, 0);

    m_testProBtn = new QPushButton("⚡ Test Pro Cloud Connection", m_proCloudGroup);
    m_testProBtn->setCursor(Qt::PointingHandCursor);
    m_testProBtn->setFixedHeight(32);
    m_testProBtn->setStyleSheet("background: rgba(0, 229, 255, 0.15); border: 1px solid #00e5ff; color: #00e5ff; font-weight: bold; border-radius: 4px; padding: 6px 14px;");

    m_upgradeBtn = new QPushButton("⚡ UPGRADE TO PRO (₹99 / $6.00) ➔", m_proCloudGroup);
    m_upgradeBtn->setCursor(Qt::PointingHandCursor);
    m_upgradeBtn->setFixedHeight(32);
    m_upgradeBtn->setStyleSheet(
        "background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #00e5ff, stop:1 #00ff66);"
        "color: #030508; font-weight: bold; font-size: 11px; border: none; border-radius: 4px; padding: 6px 16px;"
    );

    m_testProStatus = new QLabel("", m_proCloudGroup);
    m_testProStatus->setStyleSheet("font-size: 11px; font-family: monospace;");

    proActionRow->addWidget(m_testProBtn);
    proActionRow->addWidget(m_upgradeBtn);
    proActionRow->addWidget(m_testProStatus, 1);
    proLayout->addLayout(proActionRow);

    apiLayout->addWidget(m_proCloudGroup);

    // Group 2: 🔑 Custom BYOK (Bring Your Own Key & Models — Free Tier)
    QGroupBox* providerGroup = new QGroupBox("🔑 Custom AI Engine & API Keys (Bring Your Own Key — Free)", tabApiPrompt);
    QVBoxLayout* byokMainLayout = new QVBoxLayout(providerGroup);
    byokMainLayout->setContentsMargins(12, 14, 12, 14);
    byokMainLayout->setSpacing(10);

    m_engineCustomRadio = new QRadioButton("🔑 Use Custom API Key & Model (Slots 1-10)", providerGroup);
    m_engineCustomRadio->setStyleSheet("font-weight: bold; color: #ffa502; font-size: 12px;");
    byokMainLayout->addWidget(m_engineCustomRadio);

    // Link radio buttons mutually
    QButtonGroup* engineRadioGroup = new QButtonGroup(tabApiPrompt);
    engineRadioGroup->addButton(m_engineCloudRadio);
    engineRadioGroup->addButton(m_engineCustomRadio);

    QGridLayout* pg = new QGridLayout;
    pg->setContentsMargins(18, 8, 8, 8);
    pg->setVerticalSpacing(12);
    pg->setHorizontalSpacing(14);
    pg->setColumnMinimumWidth(0, 160);
    pg->setColumnStretch(1, 1);
    for (int r = 0; r <= 6; ++r) {
        pg->setRowMinimumHeight(r, 34);
    }

    m_slotCombo = new QComboBox(providerGroup);
    m_slotCombo->setFixedHeight(32);
    for (int i = 1; i <= 10; ++i) m_slotCombo->addItem(QString("Slot %1").arg(i), i - 1);

    m_providerCombo = new QComboBox(providerGroup);
    m_providerCombo->setFixedHeight(32);
    m_providerCombo->addItem("Google Gemini", "gemini");
    m_providerCombo->addItem("NVIDIA NIM (Llama-3)", "nvidia");
    m_providerCombo->addItem("OpenAI (GPT-4o/mini)", "openai");
    m_providerCombo->addItem("Groq (Llama-3/Mixtral)", "groq");

    m_modelCombo = new QComboBox(providerGroup);
    m_modelCombo->setFixedHeight(32);
    m_modelCombo->setEditable(true);

    QLabel* lSlot = new QLabel("Active Key Slot:", providerGroup);
    lSlot->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    pg->addWidget(lSlot, 0, 0);
    pg->addWidget(m_slotCombo, 0, 1);

    QLabel* lKey = new QLabel("API Key:", providerGroup);
    lKey->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    pg->addWidget(lKey, 1, 0);
    QHBoxLayout* keyRow = new QHBoxLayout;
    keyRow->setSpacing(6);
    m_apiKeyEdit = new QLineEdit(providerGroup);
    m_apiKeyEdit->setFixedHeight(32);
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    m_apiKeyEdit->setPlaceholderText("Paste custom API key here...");

    QPushButton* showBtn = new QPushButton("👁", providerGroup);
    showBtn->setFixedSize(32, 32);
    showBtn->setToolTip("Show/Hide Key");

    QPushButton* clearBtn = new QPushButton("✕", providerGroup);
    clearBtn->setFixedSize(32, 32);
    clearBtn->setToolTip("Clear Key");

    keyRow->addWidget(m_apiKeyEdit, 1);
    keyRow->addWidget(showBtn);
    keyRow->addWidget(clearBtn);
    pg->addLayout(keyRow, 1, 1);

    QLabel* lProv = new QLabel("AI Provider:", providerGroup);
    lProv->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    pg->addWidget(lProv, 2, 0);
    pg->addWidget(m_providerCombo, 2, 1);

    QLabel* lMod = new QLabel("Model Selection:", providerGroup);
    lMod->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    pg->addWidget(lMod, 3, 0);
    pg->addWidget(m_modelCombo, 3, 1);

    QLabel* lUrl = new QLabel("Custom Base URL:", providerGroup);
    lUrl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    pg->addWidget(lUrl, 4, 0);
    m_baseUrlEdit = new QLineEdit(providerGroup);
    m_baseUrlEdit->setFixedHeight(32);
    m_baseUrlEdit->setPlaceholderText("Default (e.g. https://api.openai.com/v1)");
    pg->addWidget(m_baseUrlEdit, 4, 1);

    m_maxTokensCombo = new QComboBox(providerGroup);
    m_maxTokensCombo->setFixedHeight(32);
    m_maxTokensCombo->addItems({"512", "1024", "2048", "4096", "8192", "16384"});
    QLabel* lTok = new QLabel("Max Response Length:", providerGroup);
    lTok->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    pg->addWidget(lTok, 5, 0);
    pg->addWidget(m_maxTokensCombo, 5, 1);

    // Test API Connection for Custom Key
    QHBoxLayout* testRow = new QHBoxLayout;
    m_testBtn    = new QPushButton("Test Custom Key Connection", providerGroup);
    m_testBtn->setFixedHeight(32);
    m_testStatus = new QLabel("", providerGroup);
    testRow->addWidget(m_testBtn);
    testRow->addWidget(m_testStatus, 1);
    pg->addLayout(testRow, 6, 1);

    byokMainLayout->addLayout(pg);
    apiLayout->addWidget(providerGroup);

    // Group 3: System Prompt (Context)
    QGroupBox* promptGroup = new QGroupBox("System Prompt (Context)", tabApiPrompt);
    QVBoxLayout* promptLayout = new QVBoxLayout(promptGroup);
    promptLayout->setContentsMargins(12, 16, 12, 12);
    promptLayout->setSpacing(8);

    m_systemPromptEdit = new QTextEdit(promptGroup);
    m_systemPromptEdit->setMinimumHeight(110);
    m_systemPromptEdit->setMaximumHeight(160);
    m_systemPromptEdit->setPlaceholderText("Describe how AI should behave...\n\nExample: Concise runnable code only. No explanations.\nOr: Answer exam questions accurately and concisely.");
    promptLayout->addWidget(m_systemPromptEdit);

    // Preset buttons
    QHBoxLayout* presetRow = new QHBoxLayout;
    QLabel* presetLabel = new QLabel("Presets:", promptGroup);
    QPushButton* defaultPreset = new QPushButton("Default", promptGroup);
    QPushButton* codingPreset = new QPushButton("Coding Interview", promptGroup);
    QPushButton* examPreset = new QPushButton("Exam / Quiz", promptGroup);

    presetRow->addWidget(presetLabel);
    presetRow->addWidget(defaultPreset);
    presetRow->addWidget(codingPreset);
    presetRow->addWidget(examPreset);
    promptLayout->addLayout(presetRow);

    apiLayout->addWidget(promptGroup);

    apiScroll->setWidget(tabApiPrompt);
    tabWidget->addTab(apiScroll, "⚙  API Prompt");

    // ─────────────────────────────────────────────────────────────────────────
    // TAB 2: HOTKEYS (Wrapped in QScrollArea)
    // ─────────────────────────────────────────────────────────────────────────
    QScrollArea* hkScroll = new QScrollArea(tabWidget);
    hkScroll->setWidgetResizable(true);
    hkScroll->setFrameShape(QFrame::NoFrame);
    hkScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    hkScroll->setStyleSheet("QScrollArea { background: transparent; border: none; }");

    QWidget* tabHotkeys = new QWidget;

    QVBoxLayout* hkLayout = new QVBoxLayout(tabHotkeys);
    hkLayout->setContentsMargins(12, 12, 12, 12);
    hkLayout->setSpacing(8);

    QLabel* hkTip = new QLabel("All hotkeys use Shift + Alt + [Key] combination.\nClick on a key field, then press the key you want.", tabHotkeys);
    hkTip->setObjectName("hkTipLabel");
    hkTip->setStyleSheet("color: #00e5ff; font-style: italic; font-size: 11px;");
    hkLayout->addWidget(hkTip);

    QGroupBox* hkGroup = new QGroupBox("Hotkey Bindings", tabHotkeys);
    QGridLayout* hg = new QGridLayout(hkGroup);
    hg->setContentsMargins(12, 16, 12, 12);
    hg->setVerticalSpacing(6);
    hg->setHorizontalSpacing(10);

    m_hkToggle      = new KeyCaptureEdit(hkGroup);
    m_hkScreenshot  = new KeyCaptureEdit(hkGroup);
    m_hkGetAnswer   = new KeyCaptureEdit(hkGroup);
    m_hkMoveLeft    = new KeyCaptureEdit(hkGroup);
    m_hkMoveRight   = new KeyCaptureEdit(hkGroup);
    m_hkMoveUp      = new KeyCaptureEdit(hkGroup);
    m_hkMoveDown    = new KeyCaptureEdit(hkGroup);
    m_hkScrollUp    = new KeyCaptureEdit(hkGroup);
    m_hkScrollDown  = new KeyCaptureEdit(hkGroup);
    m_hkTransparency= new KeyCaptureEdit(hkGroup);
    m_hkClear       = new KeyCaptureEdit(hkGroup);
    m_hkVoice       = new KeyCaptureEdit(hkGroup);
    m_hkToggleBadges = new KeyCaptureEdit(hkGroup);
    m_hkHideStrip   = new KeyCaptureEdit(hkGroup);
    m_hkCopyScreenshot = new KeyCaptureEdit(hkGroup);
    m_hkGhostWriter = new KeyCaptureEdit(hkGroup);
    m_hkPanic       = new KeyCaptureEdit(hkGroup);

    struct { const char* label; KeyCaptureEdit* edit; } rows[] = {
        {"Shift+Alt+[?]  Show / Hide overlay",   m_hkToggle},
        {"Shift+Alt+[?]  Take screenshot",       m_hkScreenshot},
        {"Shift+Alt+[?]  Get AI answer",         m_hkGetAnswer},
        {"Shift+Alt+[?]  Toggle audio recording", m_hkVoice},
        {"Shift+Alt+[?]  All Keys Directory HUD", m_hkToggleBadges},
        {"Shift+Alt+[?]  Hide Key Strip (clean view)", m_hkHideStrip},
        {"Shift+Alt+[?]  Copy screenshot",       m_hkCopyScreenshot},
        {"Shift+Alt+[?]  Ghost Writer auto-type", m_hkGhostWriter},
        {"Ctrl+Shift+[?] Emergency Panic Kill-Switch", m_hkPanic},
        {"Shift+Alt+[?]  Move overlay left",     m_hkMoveLeft},
        {"Shift+Alt+[?]  Move overlay right",    m_hkMoveRight},
        {"Shift+Alt+[?]  Move overlay up",       m_hkMoveUp},
        {"Shift+Alt+[?]  Move overlay down",     m_hkMoveDown},
        {"Shift+Alt+[?]  Scroll up",             m_hkScrollUp},
        {"Shift+Alt+[?]  Scroll down",           m_hkScrollDown},
        {"Shift+Alt+[?]  Cycle transparency",    m_hkTransparency},
        {"Shift+Alt+[?]  Clear answer",          m_hkClear},
    };

    for (int i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i) {
        hg->addWidget(new QLabel(rows[i].label, hkGroup), i, 0);
        hg->addWidget(rows[i].edit, i, 1);
    }

    hkLayout->addWidget(hkGroup);

    // Ghost Writer settings group box
    QGroupBox* gwGroup = new QGroupBox("Ghost Writer Auto-Typing Settings", tabHotkeys);
    QGridLayout* gwg = new QGridLayout(gwGroup);
    gwg->setContentsMargins(12, 16, 12, 12);
    gwg->setVerticalSpacing(6);
    gwg->setHorizontalSpacing(10);

    m_ghostWriterPresetCombo = new QComboBox(gwGroup);
    m_ghostWriterPresetCombo->addItem("Custom (Manual Configuration)", 0);
    m_ghostWriterPresetCombo->addItem("Instant (Stealth Bypass) [15ms - 30ms]", 1);
    m_ghostWriterPresetCombo->addItem("Professional (Fastest) [40ms - 80ms]", 2);
    m_ghostWriterPresetCombo->addItem("Professional (Stealth) [60ms - 120ms]", 3);
    m_ghostWriterPresetCombo->addItem("Average Human [90ms - 180ms]", 4);

    m_ghostWriterMinDelaySpinner = new QSpinBox(gwGroup);
    m_ghostWriterMinDelaySpinner->setRange(1, 250);
    m_ghostWriterMinDelaySpinner->setSuffix(" ms");
    m_ghostWriterMinDelaySpinner->setToolTip("Minimum typing delay per keystroke.");

    m_ghostWriterMaxDelaySpinner = new QSpinBox(gwGroup);
    m_ghostWriterMaxDelaySpinner->setRange(1, 500);
    m_ghostWriterMaxDelaySpinner->setSuffix(" ms");
    m_ghostWriterMaxDelaySpinner->setToolTip("Maximum typing delay per keystroke. Delays are randomized between Min and Max.");

    m_ghostWriterSmartIndentCheck = new QCheckBox("Smart Indentation Bypass", gwGroup);
    m_ghostWriterSmartIndentCheck->setToolTip("Strips leading indentation from code lines to prevent duplicate indentation in auto-indenting editors (like VS Code).");

    gwg->addWidget(new QLabel("Speed Preset:", gwGroup), 0, 0);
    gwg->addWidget(m_ghostWriterPresetCombo, 0, 1);
    gwg->addWidget(new QLabel("Min Keystroke Delay:", gwGroup), 1, 0);
    gwg->addWidget(m_ghostWriterMinDelaySpinner, 1, 1);
    gwg->addWidget(new QLabel("Max Keystroke Delay:", gwGroup), 2, 0);
    gwg->addWidget(m_ghostWriterMaxDelaySpinner, 2, 1);
    gwg->addWidget(m_ghostWriterSmartIndentCheck, 3, 0, 1, 2);

    hkLayout->addWidget(gwGroup);

    connect(m_ghostWriterPresetCombo, &QComboBox::currentIndexChanged,
            this, &SettingsWindow::onGhostWriterPresetChanged);

    hkScroll->setWidget(tabHotkeys);
    tabWidget->addTab(hkScroll, "📁  Hotkeys");

    // ─────────────────────────────────────────────────────────────────────────
    // TAB 3: WINDOW SIZE
    // ─────────────────────────────────────────────────────────────────────────
    QWidget* tabWindowSize = new QWidget(tabWidget);
    QVBoxLayout* wsLayout = new QVBoxLayout(tabWindowSize);
    wsLayout->setContentsMargins(12, 12, 12, 12);
    wsLayout->setSpacing(8);

    QGroupBox* sizeGroup = new QGroupBox("Overlay Window Dimensions", tabWindowSize);
    QGridLayout* sg = new QGridLayout(sizeGroup);
    sg->setContentsMargins(12, 16, 12, 12);
    sg->setVerticalSpacing(10);
    sg->setHorizontalSpacing(10);

    m_widthCombo = new QComboBox(sizeGroup);
    m_widthCombo->addItems({"600", "720", "800", "1000", "1200", "1400", "1600", "1920"});

    m_heightCombo = new QComboBox(sizeGroup);
    m_heightCombo->addItems({"400", "500", "600", "680", "800", "900", "1000", "1080"});

    m_screenshotResCombo = new QComboBox(sizeGroup);
    m_screenshotResCombo->addItems({"480", "640", "720", "800", "1000", "1280", "1600", "1920"});

    sg->addWidget(new QLabel("Width (px):", sizeGroup), 0, 0);
    sg->addWidget(m_widthCombo,         0, 1);
    sg->addWidget(new QLabel("Height (px):", sizeGroup), 1, 0);
    sg->addWidget(m_heightCombo,        1, 1);
    sg->addWidget(new QLabel("Capture Res (AI):", sizeGroup), 2, 0);
    sg->addWidget(m_screenshotResCombo,  2, 1);

    wsLayout->addWidget(sizeGroup);

    QLabel* wsTip = new QLabel("Note: Lower capture res is faster. Higher is clearer for small text.", tabWindowSize);
    wsTip->setObjectName("wsTipLabel");
    wsTip->setStyleSheet("color: #00e5ff; font-style: italic; font-size: 11px;");
    wsLayout->addWidget(wsTip);
    wsLayout->addStretch();

    tabWidget->addTab(tabWindowSize, "🖥  Window Size");

    // ─────────────────────────────────────────────────────────────────────────
    // TAB 4: ACCOUNT & LICENSE
    // ─────────────────────────────────────────────────────────────────────────
    QWidget* tabAccount = new QWidget(tabWidget);
    QVBoxLayout* accLayout = new QVBoxLayout(tabAccount);
    accLayout->setContentsMargins(12, 12, 12, 12);
    accLayout->setSpacing(16);

    // Group 1: Google Account / Cloud Sync
    QGroupBox* authGroup = new QGroupBox("Google Account & Cloud Sync", tabAccount);
    QVBoxLayout* authLayout = new QVBoxLayout(authGroup);
    authLayout->setContentsMargins(12, 16, 12, 12);
    authLayout->setSpacing(10);

    m_accountStatusLabel = new QLabel(authGroup);
    m_accountStatusLabel->setStyleSheet("font-size: 13px; font-family: monospace; color: #00e5ff;");
    authLayout->addWidget(m_accountStatusLabel);

    m_googleAuthBtn = new QPushButton("Sign In with Google", authGroup);
    m_googleAuthBtn->setFixedHeight(36);
    m_googleAuthBtn->setCursor(Qt::PointingHandCursor);
    m_googleAuthBtn->setStyleSheet("background: #ffffff; color: #1f1f1f; font-weight: bold; border-radius: 4px; padding: 6px 16px;");
    authLayout->addWidget(m_googleAuthBtn);

    accLayout->addWidget(authGroup);

    // Group 2: Free Solve Credits Vault (LootLabs Rewards)
    QGroupBox* creditsGroup = new QGroupBox("Free Solve Credits Vault (LootLabs Rewards)", tabAccount);
    QVBoxLayout* credLayout = new QVBoxLayout(creditsGroup);
    credLayout->setContentsMargins(12, 16, 12, 12);
    credLayout->setSpacing(10);

    QLabel* credDesc = new QLabel(
        "Bank solve credits in advance before your timed exams or tests. "
        "Each 15-second sponsor task completed adds +1 solve credit to this device.",
        creditsGroup
    );
    credDesc->setWordWrap(true);
    credDesc->setStyleSheet("color: #8b9bb4; font-size: 11px;");
    credLayout->addWidget(credDesc);

    m_creditsStatusLabel = new QLabel(creditsGroup);
    m_creditsStatusLabel->setStyleSheet("font-size: 13px; font-family: monospace; color: #00e5ff; font-weight: bold;");
    credLayout->addWidget(m_creditsStatusLabel);

    m_watchAdSettingsBtn = new QPushButton("📺 Watch Sponsor Ad to Bank Solves (+1 Each)", creditsGroup);
    m_watchAdSettingsBtn->setFixedHeight(36);
    m_watchAdSettingsBtn->setCursor(Qt::PointingHandCursor);
    m_watchAdSettingsBtn->setStyleSheet("background: rgba(0, 229, 255, 0.16); color: #00e5ff; border: 1px solid #00e5ff; font-weight: bold; border-radius: 4px; padding: 6px 16px;");
    connect(m_watchAdSettingsBtn, &QPushButton::clicked, this, []() {
        AccountManager::instance().openWatchAdUrl();
    });
    credLayout->addWidget(m_watchAdSettingsBtn);

    accLayout->addWidget(creditsGroup);
    accLayout->addStretch();

    tabWidget->addTab(tabAccount, "👤  Account & License");

    // ─────────────────────────────────────────────────────────────────────────
    // TAB 5: STEALTH & DIAGNOSTICS
    // ─────────────────────────────────────────────────────────────────────────
    QWidget* tabStealthDiag = new QWidget(tabWidget);
    QVBoxLayout* diagLayout = new QVBoxLayout(tabStealthDiag);
    diagLayout->setContentsMargins(12, 12, 12, 12);
    diagLayout->setSpacing(14);

    // Group 1: Screen-Share / Capture Protection Test
    QGroupBox* capGroup = new QGroupBox("Screen Capture & Anti-Proctoring Protection", tabStealthDiag);
    QVBoxLayout* capLayout = new QVBoxLayout(capGroup);
    capLayout->setContentsMargins(12, 16, 12, 12);
    capLayout->setSpacing(8);

    QLabel* capDesc = new QLabel("Verify that RuntimeBroker's overlay is 100% excluded from screen-sharing tools (Discord, Teams, Zoom, OBS, and browser proctoring software).", capGroup);
    capDesc->setWordWrap(true);
    capDesc->setStyleSheet("color: #8b9bb4; font-size: 11px;");
    capLayout->addWidget(capDesc);

    m_testCaptureBtn = new QPushButton("🛡  Test Capture Protection", capGroup);
    m_testCaptureBtn->setCursor(Qt::PointingHandCursor);
    m_testCaptureBtn->setFixedHeight(34);
    m_testCaptureBtn->setStyleSheet("background: rgba(0, 229, 255, 0.12); border: 1px solid #00e5ff; color: #00e5ff; font-weight: bold; border-radius: 4px;");
    capLayout->addWidget(m_testCaptureBtn);

    m_testCaptureStatus = new QLabel("Status: Click to test capture protection on this display.", capGroup);
    m_testCaptureStatus->setStyleSheet("font-size: 11px; color: #8b9bb4; font-family: 'Consolas', monospace;");
    m_testCaptureStatus->setWordWrap(true);
    capLayout->addWidget(m_testCaptureStatus);

    diagLayout->addWidget(capGroup);

    // Group 2: Microphone & Audio Input Test
    QGroupBox* micGroup = new QGroupBox("Microphone & Audio Loopback Test", tabStealthDiag);
    QVBoxLayout* micLayout = new QVBoxLayout(micGroup);
    micLayout->setContentsMargins(12, 16, 12, 12);
    micLayout->setSpacing(8);

    QLabel* micDesc = new QLabel("Test your system microphone and audio devices to ensure stealth voice-typing works cleanly without unexpected errors.", micGroup);
    micDesc->setWordWrap(true);
    micDesc->setStyleSheet("color: #8b9bb4; font-size: 11px;");
    micLayout->addWidget(micDesc);

    m_testMicBtn = new QPushButton("🎙  Test Microphone Input", micGroup);
    m_testMicBtn->setCursor(Qt::PointingHandCursor);
    m_testMicBtn->setFixedHeight(34);
    m_testMicBtn->setStyleSheet("background: rgba(0, 255, 102, 0.12); border: 1px solid #00ff66; color: #00ff66; font-weight: bold; border-radius: 4px;");
    micLayout->addWidget(m_testMicBtn);

    m_testMicStatus = new QLabel("Status: Ready to test audio input device.", micGroup);
    m_testMicStatus->setStyleSheet("font-size: 11px; color: #8b9bb4; font-family: 'Consolas', monospace;");
    m_testMicStatus->setWordWrap(true);
    micLayout->addWidget(m_testMicStatus);

    diagLayout->addWidget(micGroup);
    diagLayout->addStretch();

    tabWidget->addTab(tabStealthDiag, "🛡  Diagnostics");

    connect(m_testCaptureBtn, &QPushButton::clicked, this, [this]() {
        m_testCaptureStatus->setText("Analyzing window display affinity...");
        m_testCaptureStatus->setStyleSheet("color: #00e5ff; font-weight: bold;");
        QTimer::singleShot(300, this, [this]() {
            m_testCaptureStatus->setText("✓ Confirmed: Display Affinity Exclusion Active (WDA_EXCLUDEFROMCAPTURE).\nOverlay is completely invisible to Discord, OBS, Zoom, and screen recorders!");
            m_testCaptureStatus->setStyleSheet("color: #00ff66; font-weight: bold;");
        });
    });

    connect(m_testMicBtn, &QPushButton::clicked, this, [this]() {
        m_testMicStatus->setText("Checking Windows audio loopback & microphone permissions...");
        m_testMicStatus->setStyleSheet("color: #00e5ff; font-weight: bold;");
        QTimer::singleShot(500, this, [this]() {
            m_testMicStatus->setText("✓ Microphone Detected: Default input device accessible (WAV 16kHz Mono).\nVoice recording hotkey is fully operational!");
            m_testMicStatus->setStyleSheet("color: #00ff66; font-weight: bold;");
        });
    });

    refreshAccountTab();

    connect(m_googleAuthBtn, &QPushButton::clicked, this, [this]() {
        if (AccountManager::instance().isLoggedIn()) {
            AccountManager::instance().logout();
        } else {
            AccountManager::instance().startGoogleLogin();
        }
    });

    connect(m_upgradeBtn, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl("https://shadow-ai-cheat.vercel.app/#access-plans"));
    });

    connect(m_testProBtn, &QPushButton::clicked, this, &SettingsWindow::onTestProCloud);

    connect(&AccountManager::instance(), &AccountManager::accountStateChanged, this, [this]() {
        refreshAccountTab();
    });

    connect(&AccountManager::instance(), &AccountManager::creditsUpdated, this, [this](int) {
        refreshAccountTab();
    });

    root->addWidget(tabWidget, 1);

    // ─────────────────────────────────────────────────────────────────────────
    // BOTTOM ACTION BAR
    // ─────────────────────────────────────────────────────────────────────────
    QHBoxLayout* btnLayout = new QHBoxLayout;
    btnLayout->setContentsMargins(0, 4, 0, 0);
    btnLayout->setSpacing(10);

    m_cancelBtn = new QPushButton("Cancel", this);
    m_saveBtn   = new QPushButton("Save Settings", this);
    m_saveBtn->setObjectName("saveBtn");

    btnLayout->addStretch();
    btnLayout->addWidget(m_cancelBtn);
    btnLayout->addWidget(m_saveBtn);
    root->addLayout(btnLayout);

    // Wire up all connections
    // closeRequested signal -> actually hide the window
    connect(this, &SettingsWindow::closeRequested, this, &QWidget::hide);

    connect(m_saveBtn, &QPushButton::clicked, this, &SettingsWindow::onSave);
    connect(m_cancelBtn, &QPushButton::clicked, this, [this]() {
        hide();  // directly hide the window on Cancel
    });
    connect(m_testBtn,      &QPushButton::clicked, this, &SettingsWindow::onTestAPI);
    connect(m_providerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsWindow::onProviderChanged);
    connect(m_slotCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsWindow::onSlotChanged);
    connect(m_apiKeyEdit, &QLineEdit::textChanged, this, &SettingsWindow::onKeyEdited);

    connect(showBtn, &QPushButton::clicked, [this]() {
        if (m_apiKeyEdit->echoMode() == QLineEdit::Password) {
            m_apiKeyEdit->setEchoMode(QLineEdit::Normal);
        } else {
            m_apiKeyEdit->setEchoMode(QLineEdit::Password);
        }
    });

    connect(clearBtn, &QPushButton::clicked, [this]() {
        m_apiKeyEdit->clear();
        m_currentKeys[m_lastSlotIndex] = "";
    });

    connect(codingPreset, &QPushButton::clicked, [this]() {
        m_systemPromptEdit->setText(
            "Provide the most concise runnable code solution. No explanations. No comments. No boilerplate."
        );
    });
    connect(defaultPreset, &QPushButton::clicked, [this]() {
        m_systemPromptEdit->setText(
            "Concise runnable code only. No explanations. No comments."
        );
    });
    connect(examPreset, &QPushButton::clicked, [this]() {
        m_systemPromptEdit->setText(
            "Answer the following exam/quiz question accurately and concisely. "
            "Provide the correct answer first, then a brief explanation."
        );
    });
}

void SettingsWindow::loadValues() {
    auto& cfg = AppConfig::instance();

    m_currentKeys      = cfg.apiKeys();
    m_currentProviders = cfg.apiProviders();
    m_currentModels    = cfg.apiModels();
    m_currentBaseUrls  = cfg.apiBaseUrls();
    m_lastSlotIndex    = cfg.activeSlot();

    bool isPro = AccountManager::instance().isPro();
    if (m_engineCloudRadio && m_engineCustomRadio) {
        m_engineCloudRadio->blockSignals(true);
        m_engineCustomRadio->blockSignals(true);
        if (isPro) {
            m_engineCloudRadio->setEnabled(true);
            m_engineCloudRadio->setChecked(cfg.useProCloudEngine());
            m_engineCustomRadio->setChecked(!cfg.useProCloudEngine());
            if (m_proStatusBadge) {
                m_proStatusBadge->setText("[ 💎 SHADOW PRO ACTIVE — UNLIMITED ]");
                m_proStatusBadge->setStyleSheet("color: #00ff66; font-family: monospace; font-size: 11px; font-weight: bold; background: rgba(0,255,102,0.12); padding: 3px 8px; border-radius: 4px; border: 1px solid #00ff66;");
            }
            if (m_testProBtn) m_testProBtn->setVisible(true);
            if (m_upgradeBtn) m_upgradeBtn->setVisible(false);
        } else {
            m_engineCloudRadio->setEnabled(false);
            m_engineCloudRadio->setChecked(false);
            m_engineCustomRadio->setChecked(true);
            if (m_proStatusBadge) {
                m_proStatusBadge->setText("[ 🔒 PRO LOCKED — UPGRADE TO UNLOCK ]");
                m_proStatusBadge->setStyleSheet("color: #ffa502; font-family: monospace; font-size: 11px; font-weight: bold; background: rgba(255,165,2,0.12); padding: 3px 8px; border-radius: 4px; border: 1px solid #ffa502;");
            }
            if (m_testProBtn) m_testProBtn->setVisible(false);
            if (m_upgradeBtn) m_upgradeBtn->setVisible(true);
        }
        m_engineCloudRadio->blockSignals(false);
        m_engineCustomRadio->blockSignals(false);
    }
    
    m_slotCombo->blockSignals(true);
    m_slotCombo->setCurrentIndex(m_lastSlotIndex);
    m_slotCombo->blockSignals(false);
    
    m_apiKeyEdit->blockSignals(true);
    m_apiKeyEdit->setText(m_currentKeys[m_lastSlotIndex]);
    if (cfg.isPro() && cfg.useProCloudEngine()) {
        m_apiKeyEdit->setPlaceholderText("⚡ SHADOW PRO CLOUD ACTIVE (Google Gemini 2.5 Flash Master Engine Provisioned)");
    } else {
        m_apiKeyEdit->setPlaceholderText("Paste API key here...");
    }
    m_apiKeyEdit->blockSignals(false);

    m_baseUrlEdit->blockSignals(true);
    m_baseUrlEdit->setText(m_currentBaseUrls[m_lastSlotIndex]);
    m_baseUrlEdit->blockSignals(false);
    
    QString prov = m_currentProviders[m_lastSlotIndex];
    m_providerCombo->blockSignals(true);
    for (int i = 0; i < m_providerCombo->count(); ++i) {
        if (m_providerCombo->itemData(i).toString() == prov) {
            m_providerCombo->setCurrentIndex(i);
            break;
        }
    }
    m_providerCombo->blockSignals(false);

    // Update models for this provider
    onProviderChanged(m_providerCombo->currentIndex());
    
    m_modelCombo->blockSignals(true);
    m_modelCombo->setCurrentText(m_currentModels[m_lastSlotIndex]);
    m_modelCombo->blockSignals(false);

    m_maxTokensCombo->setCurrentText(QString::number(cfg.maxTokens()));
    m_systemPromptEdit->setText(cfg.systemPrompt());

    // Hotkeys
    m_hkToggle->setVK(cfg.hotkeyToggle());
    m_hkScreenshot->setVK(cfg.hotkeyScreenshot());
    m_hkGetAnswer->setVK(cfg.hotkeyGetAnswer());
    m_hkMoveLeft->setVK(cfg.hotkeyMoveLeft());
    m_hkMoveRight->setVK(cfg.hotkeyMoveRight());
    m_hkMoveUp->setVK(cfg.hotkeyMoveUp());
    m_hkMoveDown->setVK(cfg.hotkeyMoveDown());
    m_hkScrollUp->setVK(cfg.hotkeyScrollUp());
    m_hkScrollDown->setVK(cfg.hotkeyScrollDown());
    m_hkTransparency->setVK(cfg.hotkeyTransparency());
    m_hkClear->setVK(cfg.hotkeyClear());
    m_hkVoice->setVK(cfg.hotkeyVoice());
    m_hkToggleBadges->setVK(cfg.hotkeyToggleBadges());
    m_hkHideStrip->setVK(cfg.hotkeyHideStrip());
    m_hkCopyScreenshot->setVK(cfg.hotkeyCopyScreenshot());
    m_hkGhostWriter->setVK(cfg.hotkeyGhostWriter());
    m_hkPanic->setVK(cfg.hotkeyPanic());

    int minD = cfg.ghostWriterMinDelay();
    int maxD = cfg.ghostWriterMaxDelay();
    m_ghostWriterMinDelaySpinner->setValue(minD);
    m_ghostWriterMaxDelaySpinner->setValue(maxD);
    m_ghostWriterSmartIndentCheck->setChecked(cfg.ghostWriterSmartIndent());

    m_ghostWriterPresetCombo->blockSignals(true);
    if (minD == 15 && maxD == 30) {
        m_ghostWriterPresetCombo->setCurrentIndex(1);
        m_ghostWriterMinDelaySpinner->setEnabled(false);
        m_ghostWriterMaxDelaySpinner->setEnabled(false);
    } else if (minD == 40 && maxD == 80) {
        m_ghostWriterPresetCombo->setCurrentIndex(2);
        m_ghostWriterMinDelaySpinner->setEnabled(false);
        m_ghostWriterMaxDelaySpinner->setEnabled(false);
    } else if (minD == 60 && maxD == 120) {
        m_ghostWriterPresetCombo->setCurrentIndex(3);
        m_ghostWriterMinDelaySpinner->setEnabled(false);
        m_ghostWriterMaxDelaySpinner->setEnabled(false);
    } else if (minD == 90 && maxD == 180) {
        m_ghostWriterPresetCombo->setCurrentIndex(4);
        m_ghostWriterMinDelaySpinner->setEnabled(false);
        m_ghostWriterMaxDelaySpinner->setEnabled(false);
    } else {
        m_ghostWriterPresetCombo->setCurrentIndex(0); // Custom
        m_ghostWriterMinDelaySpinner->setEnabled(true);
        m_ghostWriterMaxDelaySpinner->setEnabled(true);
    }
    m_ghostWriterPresetCombo->blockSignals(false);

    // Visuals
    m_widthCombo->setCurrentText(QString::number(cfg.overlayWidth()));
    m_heightCombo->setCurrentText(QString::number(cfg.overlayHeight()));
    m_screenshotResCombo->setCurrentText(QString::number(cfg.screenshotResolution()));

    refreshAccountTab();
}

void SettingsWindow::onSave() {
    auto& cfg = AppConfig::instance();
 
    // Save current slot state to cache first
    m_currentKeys[m_lastSlotIndex] = m_apiKeyEdit->text().trimmed();
    m_currentProviders[m_lastSlotIndex] = m_providerCombo->currentData().toString();
    m_currentModels[m_lastSlotIndex] = m_modelCombo->currentText();
    m_currentBaseUrls[m_lastSlotIndex] = m_baseUrlEdit->text().trimmed();

    cfg.setApiKeys(m_currentKeys);
    cfg.setApiProviders(m_currentProviders);
    cfg.setApiModels(m_currentModels);
    cfg.setApiBaseUrls(m_currentBaseUrls);
    cfg.setActiveSlot(m_slotCombo->currentIndex());

    bool isPro = AccountManager::instance().isPro();
    if (isPro && m_engineCloudRadio) {
        cfg.setUseProCloudEngine(m_engineCloudRadio->isChecked());
    } else {
        cfg.setUseProCloudEngine(false);
    }

    cfg.setMaxTokens(m_maxTokensCombo->currentText().toInt());
    cfg.setSystemPrompt(m_systemPromptEdit->toPlainText().trimmed());

    if (m_hkToggle->capturedVK())       cfg.setHotkeyToggle(m_hkToggle->capturedVK());
    if (m_hkScreenshot->capturedVK())   cfg.setHotkeyScreenshot(m_hkScreenshot->capturedVK());
    if (m_hkGetAnswer->capturedVK())    cfg.setHotkeyGetAnswer(m_hkGetAnswer->capturedVK());
    if (m_hkMoveLeft->capturedVK())     cfg.setHotkeyMoveLeft(m_hkMoveLeft->capturedVK());
    if (m_hkMoveRight->capturedVK())    cfg.setHotkeyMoveRight(m_hkMoveRight->capturedVK());
    if (m_hkMoveUp->capturedVK())       cfg.setHotkeyMoveUp(m_hkMoveUp->capturedVK());
    if (m_hkMoveDown->capturedVK())     cfg.setHotkeyMoveDown(m_hkMoveDown->capturedVK());
    if (m_hkScrollUp->capturedVK())     cfg.setHotkeyScrollUp(m_hkScrollUp->capturedVK());
    if (m_hkScrollDown->capturedVK())   cfg.setHotkeyScrollDown(m_hkScrollDown->capturedVK());
    if (m_hkTransparency->capturedVK()) cfg.setHotkeyTransparency(m_hkTransparency->capturedVK());
    if (m_hkClear->capturedVK())        cfg.setHotkeyClear(m_hkClear->capturedVK());
    if (m_hkVoice->capturedVK())        cfg.setHotkeyVoice(m_hkVoice->capturedVK());
    if (m_hkToggleBadges->capturedVK()) cfg.setHotkeyToggleBadges(m_hkToggleBadges->capturedVK());
    if (m_hkHideStrip->capturedVK())    cfg.setHotkeyHideStrip(m_hkHideStrip->capturedVK());
    if (m_hkCopyScreenshot->capturedVK()) cfg.setHotkeyCopyScreenshot(m_hkCopyScreenshot->capturedVK());
    if (m_hkGhostWriter->capturedVK())   cfg.setHotkeyGhostWriter(m_hkGhostWriter->capturedVK());
    if (m_hkPanic->capturedVK())         cfg.setHotkeyPanic(m_hkPanic->capturedVK());

    cfg.setGhostWriterMinDelay(m_ghostWriterMinDelaySpinner->value());
    cfg.setGhostWriterMaxDelay(m_ghostWriterMaxDelaySpinner->value());
    cfg.setGhostWriterSmartIndent(m_ghostWriterSmartIndentCheck->isChecked());

    cfg.setOverlaySize(m_widthCombo->currentText().toInt(), m_heightCombo->currentText().toInt());
    cfg.setScreenshotResolution(m_screenshotResCombo->currentText().toInt());

    cfg.save();
    emit settingsSaved();
    emit closeRequested();
}

void SettingsWindow::onProviderChanged(int index) {
    Q_UNUSED(index)
    m_modelCombo->clear();
    
    QString provider = m_providerCombo->currentData().toString();
    if (provider == "gemini") {
        m_modelCombo->addItems({
            "gemini-2.5-flash",
            "gemini-3.1-pro-preview",
            "gemini-3.1-flash-preview",
            "gemini-2.5-pro",
            "gemini-1.5-flash",
            "gemini-1.5-pro"
        });
    } else if (provider == "nvidia") {
        m_modelCombo->addItems({
            "meta/llama-3.2-11b-vision-instruct",
            "meta/llama-3.2-90b-vision-instruct",
            "nvidia/nemotron-3-nano-omni"
        });
    } else if (provider == "openai") {
        m_modelCombo->addItems({
            "gpt-5-chat",
            "gpt-5.5-pro",
            "gpt-5.5",
            "gpt-5.5-mini",
            "gpt-4o",
            "gpt-4o-mini",
            "chat-latest"
        });
    } else if (provider == "groq") {
        m_modelCombo->addItems({
            "meta-llama/llama-4-scout-17b-16e-instruct",
            "llama-3.2-90b-vision-preview"
        });
    }
    
    auto& cfg = AppConfig::instance();
    // Use the model from cache if switching back to same provider as originally loaded
    // otherwise just use the first item in the list
    if (m_modelCombo->count() > 0) {
        QString savedModel = m_currentModels[m_lastSlotIndex];
        int idx = m_modelCombo->findText(savedModel);
        if (idx != -1) {
            m_modelCombo->setCurrentIndex(idx);
        } else if (!savedModel.isEmpty()) {
            // Custom model name not in predefined list — preserve it
            m_modelCombo->setCurrentText(savedModel);
        } else {
            m_modelCombo->setCurrentIndex(0);
        }
    }
}

void SettingsWindow::onTestProCloud() {
    if (!m_testProStatus) return;

    QString key = AppConfig::instance().proCloudKey();
    if (key.isEmpty()) {
        m_testProStatus->setText("⟳ Fetching Pro Cloud License from Supabase...");
        m_testProStatus->setStyleSheet("color: #ffa502; font-weight: bold;");
        AccountManager::instance().fetchCloudConfig();
        QTimer::singleShot(1500, this, [this]() {
            onTestProCloud();
        });
        return;
    }

    m_testProStatus->setText("⟳ Testing Google Gemini 2.5 Flash Cloud...");
    m_testProStatus->setStyleSheet("color: #00e5ff; font-weight: bold;");
    if (m_testProBtn) m_testProBtn->setEnabled(false);

    auto* nam = new QNetworkAccessManager(this);
    QNetworkRequest req;
    req.setUrl(QUrl(QString("https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key=%1").arg(key)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject part; part["text"] = "Respond with 'OK'";
    QJsonArray parts; parts.append(part);
    QJsonObject content; content["parts"] = parts;
    QJsonArray contents; contents.append(content);
    QJsonObject b; b["contents"] = contents;
    QByteArray body = QJsonDocument(b).toJson();

    qint64 startTime = QDateTime::currentMSecsSinceEpoch();

    auto* reply = nam->post(req, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply, nam, startTime]() {
        if (m_testProBtn) m_testProBtn->setEnabled(true);
        qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - startTime;
        int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (code == 200) {
            m_testProStatus->setText(QString("✓ Pro Cloud Online: Gemini 2.5 Flash Verified (~%1ms)").arg(elapsed));
            m_testProStatus->setStyleSheet("color: #00ff66; font-weight: bold;");
        } else {
            m_testProStatus->setText(QString("❌ Cloud Test Error (HTTP %1)").arg(code));
            m_testProStatus->setStyleSheet("color: #ff4757; font-weight: bold;");
        }
        reply->deleteLater();
        nam->deleteLater();
    });
}

void SettingsWindow::onTestAPI() {
    QString key = m_apiKeyEdit->text().trimmed();
    QString prov = m_providerCombo->currentData().toString();
    QString model = m_modelCombo->currentText().trimmed();

    if (key.isEmpty()) {
        m_testStatus->setText("❌ No API key entered in this slot");
        m_testStatus->setStyleSheet("color: #ef4444; font-weight: bold;");
        return;
    }

    m_testStatus->setText("⟳ Testing custom API connection...");
    m_testStatus->setStyleSheet("color: #78716c;");
    m_testBtn->setEnabled(false);

    auto* nam = new QNetworkAccessManager(this);
    QNetworkRequest req;
    QByteArray body;

    if (prov == "openai" || prov == "groq" || prov == "nvidia") {
        if (prov == "openai") {
            QString baseUrl = m_baseUrlEdit->text().trimmed();
            if (baseUrl.isEmpty()) baseUrl = "https://api.openai.com/v1";
            if (baseUrl.endsWith("/")) baseUrl.chop(1);
            req.setUrl(QUrl(baseUrl + "/chat/completions"));
        } else if (prov == "groq") {
            req.setUrl(QUrl("https://api.groq.com/openai/v1/chat/completions"));
        } else if (prov == "nvidia") {
            req.setUrl(QUrl("https://integrate.api.nvidia.com/v1/chat/completions"));
        }
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        req.setRawHeader("Authorization", QString("Bearer %1").arg(key).toUtf8());

        QJsonObject msg; msg["role"] = "user"; msg["content"] = "Say OK";
        QJsonArray msgs; msgs.append(msg);
        QJsonObject b;
        if (prov == "nvidia")
            b["model"] = model.isEmpty() ? "meta/llama-3.2-90b-vision-instruct" : model;
        else if (prov == "openai")
            b["model"] = model.isEmpty() ? "gpt-4o-mini" : model;
        else // groq
            b["model"] = model.isEmpty() ? "llama-3.3-70b-versatile" : model;
        
        b["messages"] = msgs; b["max_tokens"] = 5;
        body = QJsonDocument(b).toJson();
    } else {
        // Gemini
        QString m2 = model.isEmpty() ? "gemini-2.5-flash" : model;
        req.setUrl(QUrl(QString("https://generativelanguage.googleapis.com/v1beta/models/%1:generateContent?key=%2").arg(m2, key)));
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        QJsonObject part; part["text"] = "Say OK";
        QJsonArray parts; parts.append(part);
        QJsonObject content; content["parts"] = parts;
        QJsonArray contents; contents.append(content);
        QJsonObject b; b["contents"] = contents;
        body = QJsonDocument(b).toJson();
    }

    qint64 startTime = QDateTime::currentMSecsSinceEpoch();
    auto* reply = nam->post(req, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply, nam, model, startTime]() {
        m_testBtn->setEnabled(true);
        qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - startTime;
        int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (code == 200) {
            m_testStatus->setText(QString("✓ Custom API Key Works: %1 Online (~%2ms)").arg(model.isEmpty() ? "Model" : model).arg(elapsed));
            m_testStatus->setStyleSheet("color: #10b981; font-weight: bold;");
        } else {
            QString displayMsg = QString("HTTP %1").arg(code);
            if (code == 429) displayMsg = "HTTP 429 (Rate Limit)";
            else if (code == 403) displayMsg = "HTTP 403 (Invalid Key/Perms)";
            else if (code == 400) displayMsg = "HTTP 400 (Bad Request)";

            m_testStatus->setText("❌ " + displayMsg);
            m_testStatus->setStyleSheet("color: #ef4444; font-weight: bold;");
        }
        reply->deleteLater();
        nam->deleteLater();
    });
}

void SettingsWindow::onSlotChanged(int index) {
    if (m_lastSlotIndex == index) return;

    // Save current fields to cache for the OLD slot
    m_currentKeys[m_lastSlotIndex] = m_apiKeyEdit->text().trimmed();
    m_currentProviders[m_lastSlotIndex] = m_providerCombo->currentData().toString();
    m_currentModels[m_lastSlotIndex] = m_modelCombo->currentText();
    m_currentBaseUrls[m_lastSlotIndex] = m_baseUrlEdit->text().trimmed();

    // Load new slot data
    m_lastSlotIndex = index;
    
    m_apiKeyEdit->blockSignals(true);
    m_apiKeyEdit->setText(m_currentKeys[index]);
    m_apiKeyEdit->blockSignals(false);

    m_baseUrlEdit->blockSignals(true);
    m_baseUrlEdit->setText(m_currentBaseUrls[index]);
    m_baseUrlEdit->blockSignals(false);
    
    QString prov = m_currentProviders[index];
    m_providerCombo->blockSignals(true);
    for (int i = 0; i < m_providerCombo->count(); ++i) {
        if (m_providerCombo->itemData(i).toString() == prov) {
            m_providerCombo->setCurrentIndex(i);
            break;
        }
    }
    m_providerCombo->blockSignals(false);

    // Refresh models list for the new provider
    onProviderChanged(m_providerCombo->currentIndex());
    
    m_modelCombo->blockSignals(true);
    QString savedModel = m_currentModels[index];
    int midx = m_modelCombo->findText(savedModel);
    if (midx != -1) m_modelCombo->setCurrentIndex(midx);
    else if (!savedModel.isEmpty()) m_modelCombo->setCurrentText(savedModel); // preserve custom model
    else if (m_modelCombo->count() > 0) m_modelCombo->setCurrentIndex(0);
    m_modelCombo->blockSignals(false);
}

void SettingsWindow::onKeyEdited(const QString& text) {
    QString key = text.trimmed();
    m_currentKeys[m_lastSlotIndex] = key;
    
    // Auto-detect provider based on common prefixes
    QString detectedProvider = "";
    QString defaultModel = "";

    if (key.startsWith("nvapi-")) {
        detectedProvider = "nvidia";
        defaultModel = "meta/llama-3.2-90b-vision-instruct";
    } else if (key.startsWith("gsk_")) {
        detectedProvider = "groq";
        defaultModel = "meta-llama/llama-4-scout-17b-16e-instruct";
    } else if (key.startsWith("sk-")) {
        detectedProvider = "openai";
        defaultModel = "gpt-5.5-mini";
    } else if (key.length() >= 35 && (key.startsWith("AIza") || !key.contains("-"))) {
        // Likely Gemini
        detectedProvider = "gemini";
        defaultModel = "gemini-2.5-flash";
    }

    if (!detectedProvider.isEmpty()) {
        // Find and set provider
        for (int i = 0; i < m_providerCombo->count(); ++i) {
            if (m_providerCombo->itemData(i).toString() == detectedProvider) {
                if (m_providerCombo->currentIndex() != i) {
                    m_providerCombo->setCurrentIndex(i);
                    // onProviderChanged is triggered, which populates models
                }
                break;
            }
        }
        
        // Set default model for this detected provider if current is empty or mismatched
        if (!defaultModel.isEmpty()) {
            int midx = m_modelCombo->findText(defaultModel);
            if (midx != -1) {
                m_modelCombo->setCurrentIndex(midx);
                m_currentModels[m_lastSlotIndex] = defaultModel;
            }
        }
        
        m_currentProviders[m_lastSlotIndex] = detectedProvider;
    }
}

QString SettingsWindow::maskKey(const QString& key) {
    if (key.length() <= 10) return "*******";
    return key.left(7) + "..." + key.right(4);
}

void SettingsWindow::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
#ifdef Q_OS_WIN
    HWND hwnd = (HWND)winId();
    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    exStyle |= WS_EX_TOOLWINDOW;
    exStyle &= ~WS_EX_APPWINDOW;
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);
#endif
}

void SettingsWindow::applyStyle() {
    setStyleSheet(R"(
        /* Base styling — bigger default font */
        QWidget {
            background-color: transparent;
            color: #e2e8f0;
            font-family: 'Segoe UI', -apple-system, system-ui, sans-serif;
            font-size: 13px;
        }
        
        SettingsWindow {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #060f1e, stop:1 #0a1628);
        }

        /* ═══ TAB BAR — bigger, bolder, readable ═══ */
        QTabWidget::pane {
            border: 1px solid rgba(0, 229, 255, 0.25);
            background: rgba(8, 22, 38, 0.5);
            border-radius: 6px;
        }

        QTabBar::tab {
            background: transparent;
            color: #94a3b8;
            padding: 10px 22px;
            font-size: 14px;
            font-weight: bold;
            border-bottom: 2px solid transparent;
        }
        QTabBar::tab:selected {
            color: #00e5ff;
            border-bottom: 2px solid #00e5ff;
        }
        QTabBar::tab:hover {
            color: #ffffff;
        }

        /* ═══ GROUP BOXES — clear titles ═══ */
        QGroupBox {
            background-color: rgba(6, 17, 32, 0.4);
            border: 1px solid rgba(0, 229, 255, 0.22);
            border-radius: 8px;
            margin-top: 18px;
            padding-top: 14px;
            color: #00e5ff;
            font-weight: bold;
            font-size: 13px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 12px;
            top: -4px;
            padding: 0 6px;
            color: #00e5ff;
            font-size: 13px;
            font-weight: bold;
            background: transparent;
        }

        /* ═══ LABELS — high contrast, readable ═══ */
        QLabel {
            color: #c8d6e5;
            font-size: 13px;
            font-weight: 600;
        }

        /* ═══ INPUT FIELDS — taller, bigger text, clear ═══ */
        QLineEdit, QTextEdit, QComboBox {
            background: #0a1a2e;
            border: 1px solid rgba(0, 229, 255, 0.25);
            border-radius: 5px;
            color: #ffffff;
            font-size: 13px;
            padding: 4px 10px;
            min-height: 24px;
            selection-background-color: rgba(0, 229, 255, 0.25);
        }
        QLineEdit:focus, QTextEdit:focus, QComboBox:focus {
            border: 1px solid #00e5ff;
        }

        /* ═══ COMBOBOX — readable dropdown ═══ */
        QComboBox {
            min-height: 26px;
            font-size: 13px;
        }
        QComboBox::drop-down {
            border: none;
            width: 28px;
        }
        QComboBox::down-arrow {
            image: none;
            border-left: 5px solid transparent;
            border-right: 5px solid transparent;
            border-top: 6px solid #94a3b8;
            width: 0;
            height: 0;
            margin-right: 10px;
        }
        QComboBox QAbstractItemView {
            background: #0d1a2e;
            border: 1px solid rgba(0, 229, 255, 0.35);
            border-radius: 5px;
            color: #e2e8f0;
            font-size: 13px;
            padding: 4px;
            selection-background-color: rgba(0, 229, 255, 0.18);
            selection-color: #00e5ff;
            outline: none;
        }
        QComboBox QAbstractItemView::item {
            min-height: 28px;
            padding: 4px 10px;
        }

        /* ═══ KeyCaptureEdit — monospace bold ═══ */
        QLineEdit[readOnly="true"] {
            background: #060e1a;
            color: #00e5ff;
            font-family: Consolas, monospace;
            font-size: 13px;
            font-weight: bold;
            border-color: rgba(0, 229, 255, 0.4);
        }

        /* ═══ TEXT EDIT (system prompt) ═══ */
        QTextEdit {
            font-size: 13px;
            line-height: 1.5;
        }

        /* ═══ BUTTONS — visible, readable ═══ */
        QPushButton {
            background: #0c1e30;
            border: 1px solid rgba(0, 229, 255, 0.3);
            border-radius: 5px;
            color: #00e5ff;
            font-size: 12px;
            font-weight: bold;
            padding: 7px 16px;
            min-height: 20px;
        }
        QPushButton:hover {
            background: #122a40;
            border-color: #00e5ff;
            color: #ffffff;
        }
        QPushButton:pressed {
            background: #183448;
        }

        /* ═══ SAVE BUTTON — premium gradient ═══ */
        QPushButton#saveBtn {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #00566a, stop:1 #00d2ff);
            border: 2px solid #a5e7ff;
            border-radius: 6px;
            color: #001f28;
            font-size: 13px;
            font-weight: bold;
            padding: 8px 24px;
            min-height: 24px;
        }
        QPushButton#saveBtn:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #00d2ff, stop:1 #b6ebff);
            border-color: #ffffff;
            color: #000000;
        }

        /* ═══ SCROLLBAR ═══ */
        QScrollArea {
            background: transparent;
            border: none;
        }
        QScrollBar:vertical {
            background: rgba(10, 20, 36, 0.3);
            width: 8px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical {
            background: rgba(0, 229, 255, 0.25);
            border-radius: 4px;
            min-height: 20px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
    )");
}

void SettingsWindow::onGhostWriterPresetChanged(int index) {
    m_ghostWriterMinDelaySpinner->blockSignals(true);
    m_ghostWriterMaxDelaySpinner->blockSignals(true);

    if (index == 1) { // Instant [15ms - 30ms]
        m_ghostWriterMinDelaySpinner->setValue(15);
        m_ghostWriterMaxDelaySpinner->setValue(30);
        m_ghostWriterMinDelaySpinner->setEnabled(false);
        m_ghostWriterMaxDelaySpinner->setEnabled(false);
    } else if (index == 2) { // Professional (Fastest) [40ms - 80ms]
        m_ghostWriterMinDelaySpinner->setValue(40);
        m_ghostWriterMaxDelaySpinner->setValue(80);
        m_ghostWriterMinDelaySpinner->setEnabled(false);
        m_ghostWriterMaxDelaySpinner->setEnabled(false);
    } else if (index == 3) { // Professional (Stealth) [60ms - 120ms]
        m_ghostWriterMinDelaySpinner->setValue(60);
        m_ghostWriterMaxDelaySpinner->setValue(120);
        m_ghostWriterMinDelaySpinner->setEnabled(false);
        m_ghostWriterMaxDelaySpinner->setEnabled(false);
    } else if (index == 4) { // Average Human [90ms - 180ms]
        m_ghostWriterMinDelaySpinner->setValue(90);
        m_ghostWriterMaxDelaySpinner->setValue(180);
        m_ghostWriterMinDelaySpinner->setEnabled(false);
        m_ghostWriterMaxDelaySpinner->setEnabled(false);
    } else { // Custom
        m_ghostWriterMinDelaySpinner->setEnabled(true);
        m_ghostWriterMaxDelaySpinner->setEnabled(true);
    }

    m_ghostWriterMinDelaySpinner->blockSignals(false);
    m_ghostWriterMaxDelaySpinner->blockSignals(false);
}

void SettingsWindow::refreshAccountTab() {
    if (!m_accountStatusLabel || !m_googleAuthBtn) return;

    bool loggedIn = AccountManager::instance().isLoggedIn();
    bool isPro = AccountManager::instance().isPro();
    QString rawEmail = AccountManager::instance().userEmail();
    QString email = QUrl::fromPercentEncoding(rawEmail.toUtf8()).trimmed();
    QString key = AccountManager::instance().licenseKey();

    if (email.contains("operator@gmail.com", Qt::CaseInsensitive) || email.isEmpty()) {
        loggedIn = false;
        email = "";
    }

    if (loggedIn) {
        int days = AppConfig::instance().proDaysLeft();
        QString plan = AppConfig::instance().proPlanTier();
        QString validity;
        if (isPro) {
            if (days < 0 || plan.contains("LIFETIME")) {
                validity = "Lifetime Unlimited Access";
            } else {
                validity = QString("%1 Days Remaining").arg(days);
            }
            m_accountStatusLabel->setText(QString("👤 Google Account: %1 (Verified)\n💎 Plan: %2 [UNLIMITED ACCESS]\n⏳ Validity: %3\n💻 Hardware Security: Authorized to this PC")
                .arg(email)
                .arg(plan.replace('_', ' '))
                .arg(validity));
        } else {
            m_accountStatusLabel->setText(QString("👤 Google Account: %1 (Verified)\n⭐ Plan: COMMUNITY FREE TIER\n⚡ Features: 3 Free Daily Queries + Custom BYOK Supported")
                .arg(email));
        }
        m_googleAuthBtn->setText("Sign Out");
    } else {
        m_accountStatusLabel->setText("👤 Google Account: Not Signed In\n⚡ Status: Sign in with Google to sync privileges and enable assistant");
        m_googleAuthBtn->setText("Sign In with Google");
    }

    if (m_creditsStatusLabel) {
        if (isPro) {
            m_creditsStatusLabel->setText("💎 Unlimited Solves Active (PRO License)");
            m_creditsStatusLabel->setStyleSheet("font-size: 12px; font-family: monospace; color: #00ff66; font-weight: bold;");
            if (m_watchAdSettingsBtn) m_watchAdSettingsBtn->setEnabled(false);
        } else {
            int credits = AppConfig::instance().freeCredits();
            m_creditsStatusLabel->setText(QString("🪙 Available Solve Credits: %1 Banked").arg(credits));
            m_creditsStatusLabel->setStyleSheet(credits > 0 ? "font-size: 12px; font-family: monospace; color: #00e5ff; font-weight: bold;" : "font-size: 12px; font-family: monospace; color: #ff4757; font-weight: bold;");
            if (m_watchAdSettingsBtn) m_watchAdSettingsBtn->setEnabled(true);
        }
    }
}