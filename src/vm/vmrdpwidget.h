#pragma once
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QTimer>
#include <QProcess>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

// VMRdpWidget: Embeds a live Windows desktop session via local RDP loopback.
// Launches mstsc.exe connecting to 127.0.0.1 under the ShadowUser account,
// then reparents the mstsc window into this Qt widget container.
class VMRdpWidget : public QWidget {
    Q_OBJECT
public:
    explicit VMRdpWidget(QWidget* parent = nullptr);
    ~VMRdpWidget();

    void connectToLocalSession(const QString& username);
    void disconnectSession();
    bool isConnected() const { return m_connected; }

signals:
    void connected();
    void disconnected();
    void connectionFailed(const QString& reason);

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onFindTimer();

private:
    void launchMstsc(const QString& username);
    void findAndEmbedRdpWindow();

    bool      m_connected    = false;
    QString   m_username;
    QTimer*   m_findTimer    = nullptr;
    int       m_findAttempts = 0;
    QProcess* m_mstscProcess = nullptr;
    QLabel*   m_loadingLabel = nullptr;

#ifdef Q_OS_WIN
    HWND  m_rdpHwnd       = nullptr;
    HWND  m_containerHwnd = nullptr;
#endif
};
