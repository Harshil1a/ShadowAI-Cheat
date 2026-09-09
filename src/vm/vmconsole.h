#pragma once
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPropertyAnimation>
#include "vmmanager.h"
#include "vmrdpwidget.h"

class VMBrowserOverlay;
class VMScreenshotEngine;

class VMConsoleWidget : public QWidget {
    Q_OBJECT
public:
    explicit VMConsoleWidget(QWidget* parent = nullptr);
    ~VMConsoleWidget();

signals:
    void setupRequested(); // Emitted when user clicks RUN SETUP button

private slots:
    void onLaunchClicked();
    void onStopClicked();
    void onSettingsClicked();
    void onSetupClicked();
    void onVmStateChanged(VMManager::VMState newState);
    void onSandboxWindowFound(void* hwnd);
    void onRdpSessionReady(const QString& username);
    void onToggleBrowser();
    
    // Screenshot actions from browser
    void handleCaptureRequest(int mode);
    void onScreenshotCaptured(const QPixmap& pixmap);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void setupUI();
    void applyStyle();
    void animateBrowser(bool open);

    // Backend
    VMScreenshotEngine* m_screenshotEngine = nullptr;

    bool m_browserOpen = false;

    QWidget*      m_controlPanel    = nullptr;
    QPushButton*  m_launchBtn       = nullptr;
    QPushButton*  m_stopBtn         = nullptr;
    QPushButton*  m_settingsBtn     = nullptr;
    QPushButton*  m_setupBtn        = nullptr;
    QPushButton*  m_browserToggleBtn = nullptr;
    QLabel*       m_statusLabel     = nullptr;

    QWidget*      m_viewportContainer = nullptr;
    QLabel*       m_placeholderLabel = nullptr;
    QWidget*      m_embeddedContainer = nullptr;
    VMRdpWidget*  m_rdpWidget = nullptr;
    VMBrowserOverlay* m_browser     = nullptr;
};
