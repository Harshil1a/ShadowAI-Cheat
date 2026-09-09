#include "vmsettings.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QGraphicsDropShadowEffect>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>

VMSettingsDialog::VMSettingsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("System Broker — VM Settings - Runtime Broker");
    setFixedSize(500, 480);
    setWindowFlags(Qt::Window | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint);

    setupUI();
    applyStyle();
    loadSettings();
}

VMSettingsDialog::~VMSettingsDialog() {}

void VMSettingsDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(16);

    // Title
    QLabel* titleLabel = new QLabel("❖  VM CONFIGURATION MATRIX", this);
    titleLabel->setObjectName("settingsTitle");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName("settingsTabs");
    mainLayout->addWidget(m_tabs, 1);
    QWidget* displayTab = new QWidget(m_tabs);
    QFormLayout* dispLayout = new QFormLayout(displayTab);
    dispLayout->setContentsMargins(20, 20, 20, 20);
    dispLayout->setSpacing(14);

    m_resolutionCombo = new QComboBox(displayTab);
    m_resolutionCombo->addItems(QStringList() << "Auto Detect" << "1920x1080" << "1280x720" << "1024x768");
    dispLayout->addRow("Resolution:", m_resolutionCombo);

    m_fullscreenCheck = new QCheckBox("Run VM in Fullscreen Mode", displayTab);
    dispLayout->addRow("", m_fullscreenCheck);

    m_tabs->addTab(displayTab, "Display");

    // ── TAB 2: INTEGRATIONS ──────────────────────────────────────────────────
    QWidget* devTab = new QWidget(m_tabs);
    QVBoxLayout* devLayout = new QVBoxLayout(devTab);
    devLayout->setContentsMargins(20, 20, 20, 20);
    devLayout->setSpacing(14);

    m_clipboardCombo = new QComboBox(devTab);
    m_clipboardCombo->addItems(QStringList() << "Disabled" << "Bidirectional");
    m_clipboardCombo->setCurrentIndex(1); // Default to bidirectional
    
    QHBoxLayout* clipRow = new QHBoxLayout();
    clipRow->addWidget(new QLabel("Clipboard Redirection:", devTab));
    clipRow->addWidget(m_clipboardCombo, 1);
    devLayout->addLayout(clipRow);

    m_micCheck = new QCheckBox("Enable Audio Input / Microphone", devTab);
    m_micCheck->setChecked(true);
    devLayout->addWidget(m_micCheck);

    m_sharedCheck = new QCheckBox("Share Host Local Drives (*)", devTab);
    m_sharedCheck->setChecked(true);
    devLayout->addWidget(m_sharedCheck);

    m_tabs->addTab(devTab, "Integrations & Shares");

    // ── TAB 3: AUTOMATION & KEYS ─────────────────────────────────────────────
    QWidget* scTab = new QWidget(m_tabs);
    QFormLayout* scLayout = new QFormLayout(scTab);
    scLayout->setContentsMargins(20, 20, 20, 20);
    scLayout->setSpacing(14);

    m_shortcutEdit = new QLineEdit("Shift+Alt+A", scTab);
    m_shortcutEdit->setObjectName("settingsInput");
    scLayout->addRow("Chrome Hotkey:", m_shortcutEdit);
    
    QLabel* scDesc = new QLabel(
        "Set the global hotkey configuration to launch or toggle the Google Chrome "
        "browser overlay directly inside your active VM screen instantly.", scTab
    );
    scDesc->setWordWrap(true);
    scDesc->setStyleSheet("color: #64748b; font-size: 11px; margin-top: 10px;");
    scLayout->addRow("", scDesc);

    m_tabs->addTab(scTab, "Automation");

    // ── DIALOG BUTTONS ───────────────────────────────────────────────────────
    QWidget* buttonsBar = new QWidget(this);
    QHBoxLayout* bLayout = new QHBoxLayout(buttonsBar);
    bLayout->setContentsMargins(0, 0, 0, 0);
    bLayout->setSpacing(12);
    bLayout->addStretch();

    m_saveBtn = new QPushButton("SAVE SETTINGS", buttonsBar);
    m_saveBtn->setObjectName("saveBtn");
    m_saveBtn->setFixedHeight(36);
    m_saveBtn->setFixedWidth(140);
    m_saveBtn->setCursor(Qt::PointingHandCursor);
    bLayout->addWidget(m_saveBtn);

    m_cancelBtn = new QPushButton("CANCEL", buttonsBar);
    m_cancelBtn->setObjectName("cancelBtn");
    m_cancelBtn->setFixedHeight(36);
    m_cancelBtn->setFixedWidth(100);
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    bLayout->addWidget(m_cancelBtn);

    mainLayout->addWidget(buttonsBar);

    // Connections
    connect(m_saveBtn, &QPushButton::clicked, this, &VMSettingsDialog::onSave);
    connect(m_cancelBtn, &QPushButton::clicked, this, &VMSettingsDialog::onCancel);
}

void VMSettingsDialog::loadSettings() {
    QSettings settings("Microsoft", "RuntimeBroker");
    m_resolutionCombo->setCurrentText(settings.value("vm/resolution", "Auto Detect").toString());
    m_fullscreenCheck->setChecked(settings.value("vm/fullscreen", false).toBool());
    
    int clipVal = settings.value("vm/clipboardMode", 1).toInt(); // default 1 (enabled)
    m_clipboardCombo->setCurrentIndex(clipVal == 0 ? 0 : 1);
    
    m_micCheck->setChecked(settings.value("vm/micEnabled", true).toBool());
    m_sharedCheck->setChecked(settings.value("vm/sharedEnabled", true).toBool());
    m_shortcutEdit->setText(settings.value("vm/shortcut", "Shift+Alt+A").toString());
}

void VMSettingsDialog::onSave() {
    QSettings settings("Microsoft", "RuntimeBroker");
    settings.setValue("vm/resolution", m_resolutionCombo->currentText());
    settings.setValue("vm/fullscreen", m_fullscreenCheck->isChecked());
    
    int clipVal = m_clipboardCombo->currentIndex() == 0 ? 0 : 1;
    settings.setValue("vm/clipboardMode", clipVal);
    
    settings.setValue("vm/micEnabled", m_micCheck->isChecked());
    settings.setValue("vm/sharedEnabled", m_sharedCheck->isChecked());
    settings.setValue("vm/shortcut", m_shortcutEdit->text().trimmed());

    accept();
}

void VMSettingsDialog::onCancel() {
    reject();
}

void VMSettingsDialog::applyStyle() {
    setStyleSheet(R"(
        VMSettingsDialog {
            background: #060a16;
            color: #8b9bb4;
            font-family: 'Segoe UI', sans-serif;
        }
        QLabel {
            color: #8b9bb4;
            font-size: 12px;
        }
        QLabel#settingsTitle {
            color: #00e5ff;
            font-size: 15px;
            font-weight: bold;
            letter-spacing: 2px;
            margin-bottom: 5px;
        }
        QTabWidget#settingsTabs::pane {
            border: 1px solid rgba(0, 229, 255, 0.15);
            background: rgba(14, 24, 44, 0.4);
            border-radius: 6px;
        }
        QTabBar::tab {
            background: rgba(14, 24, 44, 0.6);
            border: 1px solid rgba(0, 229, 255, 0.1);
            border-bottom: none;
            color: #8b9bb4;
            padding: 8px 16px;
            margin-right: 2px;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
            font-size: 11px;
            font-weight: bold;
        }
        QTabBar::tab:selected {
            background: rgba(14, 24, 44, 0.9);
            border: 1px solid rgba(0, 229, 255, 0.22);
            border-bottom: none;
            color: #00e5ff;
        }
        QTabBar::tab:hover {
            color: #ffffff;
            background: rgba(0, 229, 255, 0.05);
        }
        QLineEdit#settingsInput {
            background: rgba(4, 8, 16, 0.8);
            border: 1px solid rgba(0, 229, 255, 0.2);
            border-radius: 4px;
            color: #e5e7eb;
            font-size: 12px;
            padding: 4px 8px;
        }
        QLineEdit#settingsInput:focus {
            border: 1px solid rgba(0, 229, 255, 0.45);
        }
        QComboBox, QSpinBox {
            background: rgba(4, 8, 16, 0.8);
            border: 1px solid rgba(0, 229, 255, 0.2);
            border-radius: 4px;
            color: #e5e7eb;
            padding: 4px 8px;
            font-size: 12px;
        }
        QComboBox::drop-down {
            border: none;
        }
        QCheckBox {
            color: #8b9bb4;
            font-size: 12px;
            spacing: 6px;
        }
        QCheckBox::indicator {
            width: 14px;
            height: 14px;
            background: rgba(4, 8, 16, 0.8);
            border: 1px solid rgba(0, 229, 255, 0.25);
            border-radius: 3px;
        }
        QCheckBox::indicator:checked {
            background: #00e5ff;
            border: 1px solid #00e5ff;
        }
        QListWidget#sharedList {
            background: rgba(4, 8, 16, 0.8);
            border: 1px solid rgba(0, 229, 255, 0.2);
            border-radius: 4px;
            color: #d1d5db;
            font-size: 11px;
            padding: 4px;
        }
        QPushButton#smallBtn {
            background: rgba(14, 24, 44, 0.8);
            border: 1px solid rgba(0, 229, 255, 0.2);
            border-radius: 4px;
            color: #8b9bb4;
            font-size: 11px;
            font-weight: bold;
        }
        QPushButton#smallBtn:hover {
            color: #ffffff;
            background: rgba(0, 229, 255, 0.08);
            border: 1px solid rgba(0, 229, 255, 0.4);
        }
        QPushButton#saveBtn {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #0a2e3f, stop:1 #071e2c);
            border-top: 1.5px solid rgba(0, 229, 255, 0.65);
            border-left: 1.5px solid rgba(0, 229, 255, 0.4);
            border-radius: 6px;
            color: #00e5ff;
            font-size: 12px;
            font-weight: bold;
            letter-spacing: 1px;
        }
        QPushButton#saveBtn:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #0f3d54, stop:1 #0a2a3e);
            color: #ffffff;
            border-top: 1.5px solid rgba(0, 229, 255, 0.85);
        }
        QPushButton#cancelBtn {
            background: rgba(14, 24, 44, 0.6);
            border: 1px solid rgba(0, 229, 255, 0.15);
            border-radius: 6px;
            color: #8b9bb4;
            font-size: 12px;
            font-weight: bold;
            letter-spacing: 1px;
        }
        QPushButton#cancelBtn:hover {
            background: rgba(14, 24, 44, 0.9);
            color: #ffffff;
            border: 1px solid rgba(0, 229, 255, 0.3);
        }
    )");
}
