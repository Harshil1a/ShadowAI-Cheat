#pragma once
#include <QDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QTabWidget>

class VMSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit VMSettingsDialog(QWidget* parent = nullptr);
    ~VMSettingsDialog();

private slots:
    void onSave();
    void onCancel();

private:
    void setupUI();
    void applyStyle();
    void loadSettings();

    QTabWidget*   m_tabs            = nullptr;

    // Display Tab
    QComboBox*    m_resolutionCombo = nullptr;
    QCheckBox*    m_fullscreenCheck = nullptr;

    // Clipboard Tab
    QComboBox*    m_clipboardCombo  = nullptr;

    // Devices & Shared Folders Tab
    QCheckBox*    m_micCheck        = nullptr;
    QCheckBox*    m_sharedCheck     = nullptr;

    // Shortcut/Automation Tab
    QLineEdit*    m_shortcutEdit    = nullptr;

    QPushButton*  m_saveBtn         = nullptr;
    QPushButton*  m_cancelBtn       = nullptr;
};
