#include "vmrdpwidget.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QDebug>
#include <QTimer>
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QApplication>
#include <QSettings>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

VMRdpWidget::VMRdpWidget(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_DontCreateNativeAncestors);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_loadingLabel = new QLabel(this);
    m_loadingLabel->setText(
        "Connecting to local session...\n\nPlease wait while the remote desktop session starts."
    );
    m_loadingLabel->setAlignment(Qt::AlignCenter);
    m_loadingLabel->setStyleSheet(
        "color: #00e5ff; font-size: 14px; font-family: 'Segoe UI'; background: transparent;"
    );
    layout->addWidget(m_loadingLabel, 1, Qt::AlignCenter);

    m_findTimer = new QTimer(this);
    m_findTimer->setInterval(1000);
    connect(m_findTimer, &QTimer::timeout, this, &VMRdpWidget::onFindTimer);
}

VMRdpWidget::~VMRdpWidget() {
    disconnectSession();
}

void VMRdpWidget::connectToLocalSession(const QString& username) {
    m_username     = username;
    m_connected    = false;
    m_findAttempts = 0;
    m_rdpHwnd      = nullptr;

    m_loadingLabel->setText(
        QString("Starting local session for: %1\n\nConnecting via 127.0.0.1...").arg(username)
    );
    m_loadingLabel->show();

    launchMstsc(username);
    m_findTimer->start();
}

void VMRdpWidget::launchMstsc(const QString& username) {
#ifdef Q_OS_WIN
    // Automatically save RDP credentials using cmdkey for both targets so connection is fully automatic
    QProcess cmdkey1;
    cmdkey1.start("cmdkey", QStringList() << "/generic:127.0.0.1" << ("/user:" + username) << "/pass:ShadowAI@2024");
    cmdkey1.waitForFinished(3000);

    QProcess cmdkey2;
    cmdkey2.start("cmdkey", QStringList() << "/generic:TERMSRV/127.0.0.1" << ("/user:" + username) << "/pass:ShadowAI@2024");
    cmdkey2.waitForFinished(3000);

    // Suppress connection bar in registry before launch
    QProcess regBar;
    regBar.start("reg", QStringList() << "add" << "HKCU\\Software\\Microsoft\\Terminal Server Client" << "/v" << "HideConnectionBar" << "/t" << "REG_DWORD" << "/d" << "1" << "/f");
    regBar.waitForFinished(2000);
#endif

    QSettings settings("Microsoft", "RuntimeBroker");

    // Write hotkey configuration for the session helper
    QFile configFile("C:/Users/Public/ShadowAI_config.txt");
    if (configFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&configFile);
        out << settings.value("vm/shortcut", "Shift+Alt+A").toString().trimmed();
        configFile.close();
    }

    // 1. Resolution & Size
    QString resStr = settings.value("vm/resolution", "Auto Detect").toString();
    int desktopWidth = width();
    int desktopHeight = height();
    if (resStr != "Auto Detect" && resStr.contains('x')) {
        QStringList parts = resStr.split('x');
        if (parts.size() == 2) {
            desktopWidth = parts[0].toInt();
            desktopHeight = parts[1].toInt();
        }
    }

    // 2. Fullscreen vs Windowed
    bool fullscreen = settings.value("vm/fullscreen", false).toBool();
    int screenMode = fullscreen ? 2 : 1; // 2 = Fullscreen, 1 = Windowed

    // 3. Clipboard Mode
    int clipboardMode = settings.value("vm/clipboardMode", 1).toInt(); // 0 = disabled, 1 = bidirectional
    int redirectClipboard = (clipboardMode != 0) ? 1 : 0;

    // 4. Microphone / Audio Redirection
    bool micEnabled = settings.value("vm/micEnabled", true).toBool();
    int redirectMic = micEnabled ? 1 : 0;

    // 5. Shared Folder (Drive Redirection)
    bool sharedEnabled = settings.value("vm/sharedEnabled", true).toBool();

    QString rdpFilePath = QDir::toNativeSeparators(QDir::tempPath() + "/shadowai_local.rdp");

    QFile rdpFile(rdpFilePath);
    if (rdpFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&rdpFile);
        out << "full address:s:127.0.0.1\n";
        out << "username:s:" << username << "\n";
        out << "authentication level:i:0\n";
        out << "prompt for credentials:i:0\n";
        out << "negotiate security layer:i:1\n";
        out << "enablecredsspsupport:i:1\n";
        out << "desktopwidth:i:" << desktopWidth << "\n";
        out << "desktopheight:i:" << desktopHeight << "\n";
        out << "screen mode id:i:" << screenMode << "\n";
        out << "redirectclipboard:i:" << redirectClipboard << "\n";
        out << "audiomode:i:0\n"; // Play audio on host
        out << "audiocapturemode:i:" << redirectMic << "\n"; // Redirect microphone
        out << "redirectprinters:i:0\n";
        out << "redirectcomports:i:0\n";
        out << "redirectsmartcards:i:0\n";
        if (sharedEnabled) {
            out << "drivestoredirect:s:*\n"; // Share host drives to the session
        } else {
            out << "drivestoredirect:s:\n";
        }
        out << "use multimon:i:0\n";
        out << "disable wallpaper:i:0\n";
        out << "allow font smoothing:i:1\n";
        out << "allow desktop composition:i:1\n";
        out << "smart sizing:i:1\n";
        out << "displayconnectionbar:i:0\n";
        out << "pinconnectionbar:i:0\n";
        out << "bandwidthautodetect:i:1\n";
        out << "connection type:i:7\n";
        rdpFile.close();
    }

    if (m_mstscProcess) {
        m_mstscProcess->kill();
        m_mstscProcess->deleteLater();
        m_mstscProcess = nullptr;
    }

    m_mstscProcess = new QProcess(this);
    m_mstscProcess->setProgram("mstsc.exe");
    m_mstscProcess->setArguments(QStringList() << rdpFilePath);
    m_mstscProcess->start();

    connect(m_mstscProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        emit connectionFailed(QString("mstsc.exe failed to start (error %1).").arg((int)e));
        m_findTimer->stop();
    });
}

struct FindRdpData {
    HWND  found = nullptr;
    DWORD pid   = 0;
};

static BOOL CALLBACK FindRdpWindowProc(HWND hwnd, LPARAM lParam) {
    FindRdpData* data = reinterpret_cast<FindRdpData*>(lParam);
    wchar_t cls[256]   = {};
    wchar_t title[512] = {};
    GetClassNameW(hwnd, cls, 256);
    GetWindowTextW(hwnd, title, 512);

    std::wstring c(cls), t(title);

    if ((c == L"TscShellContainerClass" || c == L"UIMainClass" ||
         c.find(L"Mstsc") != std::wstring::npos ||
         t.find(L"Remote Desktop Connection") != std::wstring::npos ||
         t.find(L"127.0.0.1") != std::wstring::npos) && IsWindowVisible(hwnd)) {
        data->found = hwnd;
        return FALSE;
    }
    return TRUE;
}

void VMRdpWidget::findAndEmbedRdpWindow() {
#ifdef Q_OS_WIN
    FindRdpData data;
    if (m_mstscProcess && m_mstscProcess->state() == QProcess::Running) {
        data.pid = (DWORD)m_mstscProcess->processId();
    }
    EnumWindows(FindRdpWindowProc, reinterpret_cast<LPARAM>(&data));
    if (!data.found) return;

    m_rdpHwnd       = data.found;
    m_containerHwnd = (HWND)winId();

    SetParent(m_rdpHwnd, m_containerHwnd);

    LONG style = GetWindowLong(m_rdpHwnd, GWL_STYLE);
    style &= ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
    style |= WS_CHILD;
    SetWindowLong(m_rdpHwnd, GWL_STYLE, style);

    LONG exStyle = GetWindowLong(m_rdpHwnd, GWL_EXSTYLE);
    exStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
    SetWindowLong(m_rdpHwnd, GWL_EXSTYLE, exStyle);

    RECT rect;
    GetClientRect(m_containerHwnd, &rect);
    SetWindowPos(m_rdpHwnd, HWND_TOP, 0, 0, rect.right, rect.bottom, SWP_SHOWWINDOW | SWP_FRAMECHANGED);

    m_findTimer->stop();
    m_connected = true;
    m_loadingLabel->hide();
    emit connected();

    qDebug() << "[VMRdpWidget] RDP window embedded successfully.";
#endif
}

void VMRdpWidget::onFindTimer() {
    m_findAttempts++;
    findAndEmbedRdpWindow();

    if (!m_connected && m_findAttempts > 30) {
        m_findTimer->stop();
        emit connectionFailed(
            "Could not find mstsc window after 30 seconds. "
            "Make sure RDP is enabled and ShadowUser exists."
        );
    }
}

void VMRdpWidget::disconnectSession() {
    m_findTimer->stop();
    m_connected = false;

#ifdef Q_OS_WIN
    if (m_rdpHwnd && IsWindow(m_rdpHwnd)) {
        SetParent(m_rdpHwnd, nullptr);
        PostMessage(m_rdpHwnd, WM_CLOSE, 0, 0);
        m_rdpHwnd = nullptr;
    }
    QProcess::startDetached("taskkill", QStringList() << "/f" << "/im" << "mstsc.exe");
#endif

    if (m_mstscProcess) {
        m_mstscProcess->kill();
        m_mstscProcess->waitForFinished(2000);
        m_mstscProcess->deleteLater();
        m_mstscProcess = nullptr;
    }

    m_loadingLabel->show();
    m_loadingLabel->setText("Session disconnected.");
    emit disconnected();
}

void VMRdpWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
#ifdef Q_OS_WIN
    if (m_rdpHwnd && IsWindow(m_rdpHwnd) && m_containerHwnd) {
        RECT rect;
        GetClientRect(m_containerHwnd, &rect);
        SetWindowPos(m_rdpHwnd, nullptr, 0, 0, rect.right, rect.bottom, SWP_NOZORDER | SWP_NOACTIVATE);
    }
#endif
}
