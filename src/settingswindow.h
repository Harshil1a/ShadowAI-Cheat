#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QTextEdit>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QMap>
#include <QSpinBox>
#include <QKeyEvent>
#include <QCheckBox>

// Custom widget for capturing a single key (for hotkey remapping)
class KeyCaptureEdit : public QLineEdit {
    Q_OBJECT
public:
    explicit KeyCaptureEdit(QWidget* parent = nullptr);
    int capturedVK() const { return m_vk; }
    void setVK(int vk);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    int m_vk = 0;
    static QString vkToName(int vk);
};

class SettingsWindow : public QWidget {
    Q_OBJECT
public:
    explicit SettingsWindow(QWidget* parent = nullptr);

signals:
    void settingsSaved();
    void closeRequested();

private slots:
    void onSave();
    void onProviderChanged(int index);
    void onTestAPI();
    void onSlotChanged(int index);
    void onKeyEdited(const QString& text);
    void onGhostWriterPresetChanged(int index);

private:
    void setupUI();
    void loadValues();
    void applyStyle();
    QString maskKey(const QString& key);

protected:
    void showEvent(QShowEvent* event) override;

    // API settings
    QComboBox*  m_engineModeCombo = nullptr;
    QComboBox*  m_slotCombo;
    QLineEdit*  m_apiKeyEdit;
    QLineEdit*  m_baseUrlEdit;
    QStringList  m_currentKeys;
    QStringList  m_currentProviders;
    QStringList  m_currentModels;
    QStringList  m_currentBaseUrls;
    int          m_lastSlotIndex = 0;

    QComboBox*  m_providerCombo;
    QComboBox*  m_modelCombo;
    QComboBox*  m_maxTokensCombo;
    QComboBox*  m_widthCombo;
    QComboBox*  m_heightCombo;
    QComboBox*  m_screenshotResCombo;

    QTextEdit*  m_systemPromptEdit;

    // Hotkey settings
    KeyCaptureEdit* m_hkToggle;
    KeyCaptureEdit* m_hkScreenshot;
    KeyCaptureEdit* m_hkGetAnswer;
    KeyCaptureEdit* m_hkMoveLeft;
    KeyCaptureEdit* m_hkMoveRight;
    KeyCaptureEdit* m_hkMoveUp;
    KeyCaptureEdit* m_hkMoveDown;
    KeyCaptureEdit* m_hkScrollUp;
    KeyCaptureEdit* m_hkScrollDown;
    KeyCaptureEdit* m_hkTransparency;
    KeyCaptureEdit* m_hkClear;
    KeyCaptureEdit* m_hkVoice;
    KeyCaptureEdit* m_hkToggleBadges;
    KeyCaptureEdit* m_hkHideStrip;

    KeyCaptureEdit* m_hkCopyScreenshot;
    KeyCaptureEdit* m_hkGhostWriter;
    KeyCaptureEdit* m_hkPanic;

    QSpinBox*   m_ghostWriterMinDelaySpinner;
    QSpinBox*   m_ghostWriterMaxDelaySpinner;
    QCheckBox*  m_ghostWriterSmartIndentCheck;
    QComboBox*  m_ghostWriterPresetCombo;

    QPushButton* m_saveBtn;
    QPushButton* m_cancelBtn;
    QPushButton* m_testBtn;
    QLabel*      m_testStatus;

    QPushButton* m_testCaptureBtn = nullptr;
    QLabel*      m_testCaptureStatus = nullptr;
    QPushButton* m_testMicBtn = nullptr;
    QLabel*      m_testMicStatus = nullptr;

    // Account & License UI
    QLabel*      m_accountStatusLabel = nullptr;
    QPushButton* m_googleAuthBtn      = nullptr;
    QPushButton* m_upgradeBtn         = nullptr;
    QLabel*      m_licenseFeedback    = nullptr;
    void refreshAccountTab();
};
