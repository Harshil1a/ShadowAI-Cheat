#pragma once
#include <QObject>
#include <QString>
#include <QProcess>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class VMManager : public QObject {
    Q_OBJECT
public:
    enum VMState {
        Stopped,
        Starting,
        Running,
        Stopping,
        Error
    };
    Q_ENUM(VMState)

    static VMManager& instance();

    // --- Sandbox (legacy, kept for compatibility) ---
    bool isSandboxAvailable() const;
    bool isSandboxEnabled() const;
    bool enableSandboxFeature();

    // --- RDP Loopback VM Mode ---
    bool isRdpReady() const;          // Check if ShadowUser + RDP service are ready
    bool setupRdpUser();              // Create ShadowUser (passwordless) via net user
    bool enableRdpAccess();           // Enable Remote Desktop in registry
    bool addDefenderExclusion();      // Add Windows Defender exclusion for RDP Wrapper
    bool installRdpWrapper();         // Download and install RDP Wrapper
    QString rdpUsername() const { return "ShadowUser"; }

    // --- VM lifecycle ---
    bool startVM();
    void stopVM();
    VMState state() const { return m_state; }

    HWND getSandboxWindow() const { return m_sandboxHwnd; }
    QString getSharedFolderPath() const;
    QString getWsbFilePath() const;

signals:
    void stateChanged(VMState newState);
    void logMessage(const QString& msg, bool isError = false);
    void sandboxWindowFound(void* hwnd);
    void rdpSessionReady(const QString& username);   // Emitted when RDP is ready to connect

private slots:
    void checkSandboxProcess();

private:
    explicit VMManager(QObject* parent = nullptr);
    ~VMManager();

    void setState(VMState s);
    bool generateWsbFile();
    bool writeStartupScript();
    bool runElevatedCommand(const QString& command);

    VMState   m_state = Stopped;
    QProcess* m_process = nullptr;
    QTimer*   m_monitorTimer = nullptr;
    HWND      m_sandboxHwnd = nullptr;
    int       m_findAttempts = 0;
};

