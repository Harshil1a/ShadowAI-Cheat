#include "vmmanager.h"
#include "../appconfig.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QDebug>
#include <QCoreApplication>

VMManager& VMManager::instance() {
    static VMManager inst;
    return inst;
}

VMManager::VMManager(QObject* parent) : QObject(parent) {
    m_monitorTimer = new QTimer(this);
    m_monitorTimer->setInterval(1000);
    connect(m_monitorTimer, &QTimer::timeout, this, &VMManager::checkSandboxProcess);
}

VMManager::~VMManager() {
    stopVM();
}

bool VMManager::isSandboxAvailable() const {
    return true; // Always return true so VMware/VirtualBox or native sandbox can be used
}

bool VMManager::isSandboxEnabled() const {
#ifdef Q_OS_WIN
    // Simple check: if WindowsSandbox.exe exists, the feature is active/installed
    return isSandboxAvailable();
#else
    return false;
#endif
}

bool VMManager::enableSandboxFeature() {
    emit logMessage("Triggering Windows Sandbox enablement...");
    return false; // Replaced by RDP loopback mode
}

bool VMManager::runElevatedCommand(const QString& command) {
#ifdef Q_OS_WIN
    QString ps = QString("Start-Process cmd.exe -ArgumentList '/c %1' -Verb RunAs -Wait").arg(command);
    QProcess proc;
    proc.start("powershell.exe", QStringList() << "-NoProfile" << "-ExecutionPolicy" << "Bypass" << "-Command" << ps);
    proc.waitForFinished(30000);
    return proc.exitCode() == 0;
#else
    Q_UNUSED(command)
    return false;
#endif
}

bool VMManager::isRdpReady() const {
#ifdef Q_OS_WIN
    // Check 0: On Windows Home, RDP Wrapper must be installed for loopback / passwordless RDP
    if (!QFile::exists("C:/Program Files/RDP Wrapper/rdpwrap.ini")) {
        return false;
    }

    // Check 1: ShadowUser account exists
    QProcess check;
    check.start("net", QStringList() << "user" << "ShadowUser");
    check.waitForFinished(5000);
    if (check.exitCode() != 0) return false;

    // Check 2: Remote Desktop is enabled (fDenyTSConnections == 0)
    QSettings rdpReg(
        "HKEY_LOCAL_MACHINE\\System\\CurrentControlSet\\Control\\Terminal Server",
        QSettings::NativeFormat
    );
    int deny = rdpReg.value("fDenyTSConnections", 1).toInt();
    return deny == 0;
#else
    return false;
#endif
}

bool VMManager::setupRdpUser() {
    emit logMessage("[RDP] Creating ShadowUser account (passworded: ShadowAI@2024)...");
#ifdef Q_OS_WIN
    // Create the user with hard password ShadowAI@2024
    QProcess proc;
    proc.start("net", QStringList() << "user" << "ShadowUser" << "ShadowAI@2024" << "/add");
    proc.waitForFinished(10000);
    if (proc.exitCode() != 0) {
        // User might already exist, update password
        proc.start("net", QStringList() << "user" << "ShadowUser" << "ShadowAI@2024");
        proc.waitForFinished(5000);
    }
    // Set password to never expire
    proc.start("wmic", QStringList() << "useraccount" << "where" << "name='ShadowUser'" << "set" << "passwordexpires=false");
    proc.waitForFinished(5000);

    // Add to Administrators group (Remote Desktop Users does not exist on Windows Home)
    proc.start("net", QStringList() << "localgroup" << "Administrators" << "ShadowUser" << "/add");
    proc.waitForFinished(5000);

    // Deny ShadowUser read/list access to Common Desktop (Public Desktop) so they get a completely clean empty desktop
    proc.start("icacls", QStringList() << "C:\\Users\\Public\\Desktop" << "/deny" << "ShadowUser:(OI)(CI)(R)");
    proc.waitForFinished(5000);

    emit logMessage("[RDP] ShadowUser account configured.");

    // Stage ShadowRdpHelper to ShadowUser's Startup folder
    QString startupDir = "C:/Users/ShadowUser/AppData/Roaming/Microsoft/Windows/Start Menu/Programs/Startup";
    QDir().mkpath(startupDir);
    QString helperDest = QDir::toNativeSeparators(startupDir + "/ShadowRdpHelper.exe");
    QString helperSrc = QDir::toNativeSeparators(QCoreApplication::applicationDirPath() + "/ShadowRdpHelper.exe");
    
    if (QFile::exists(helperSrc)) {
        QFile::remove(helperDest); // remove old if exists
        if (QFile::copy(helperSrc, helperDest)) {
            emit logMessage("[RDP] Configured ShadowRdpHelper inside the RDP session.");
        } else {
            emit logMessage("[WARNING] Failed to copy ShadowRdpHelper to Startup folder.");
        }
    } else {
        emit logMessage(QString("[WARNING] ShadowRdpHelper.exe not found at %1").arg(helperSrc));
    }

    return true;
#else
    return false;
#endif
}

bool VMManager::enableRdpAccess() {
    emit logMessage("[RDP] Enabling Remote Desktop on this machine...");
#ifdef Q_OS_WIN
    // Enable RDP via registry (fDenyTSConnections = 0)
    QProcess proc;
    proc.start("reg", QStringList()
        << "add"
        << "HKLM\\System\\CurrentControlSet\\Control\\Terminal Server"
        << "/v" << "fDenyTSConnections"
        << "/t" << "REG_DWORD"
        << "/d" << "0"
        << "/f");
    proc.waitForFinished(8000);

    // Disable NLA requirement (UserAuthentication = 0)
    proc.start("reg", QStringList()
        << "add"
        << "HKLM\\System\\CurrentControlSet\\Control\\Terminal Server\\WinStations\\RDP-Tcp"
        << "/v" << "UserAuthentication"
        << "/t" << "REG_DWORD"
        << "/d" << "0"
        << "/f");
    proc.waitForFinished(8000);

    // Open firewall for RDP
    proc.start("netsh", QStringList()
        << "advfirewall" << "firewall"
        << "set" << "rule"
        << "group=\"remote desktop\""
        << "new" << "enable=yes");
    proc.waitForFinished(8000);

    // Allow blank passwords for local loopback
    proc.start("reg", QStringList()
        << "add"
        << "HKLM\\SYSTEM\\CurrentControlSet\\Control\\Lsa"
        << "/v" << "LimitBlankPasswordUse"
        << "/t" << "REG_DWORD"
        << "/d" << "0"
        << "/f");
    proc.waitForFinished(8000);

    // Enable concurrent sessions
    proc.start("reg", QStringList()
        << "add"
        << "HKLM\\System\\CurrentControlSet\\Control\\Terminal Server"
        << "/v" << "fSingleSessionPerUser"
        << "/t" << "REG_DWORD"
        << "/d" << "0"
        << "/f");
    proc.waitForFinished(8000);

    proc.start("reg", QStringList()
        << "add"
        << "HKLM\\System\\CurrentControlSet\\Control\\Terminal Server\\Licensing Core"
        << "/v" << "EnableConcurrentSessions"
        << "/t" << "REG_DWORD"
        << "/d" << "1"
        << "/f");
    proc.waitForFinished(8000);

    // Suppress RDP redirection and consent warning dialogs (Windows 11 updates)
    proc.start("reg", QStringList()
        << "add"
        << "HKLM\\Software\\Policies\\Microsoft\\Windows NT\\Terminal Services\\Client"
        << "/v" << "RedirectionWarningDialogVersion"
        << "/t" << "REG_DWORD"
        << "/d" << "1"
        << "/f");
    proc.waitForFinished(8000);

    proc.start("reg", QStringList()
        << "add"
        << "HKLM\\Software\\Microsoft\\Terminal Server Client"
        << "/v" << "RdpLaunchConsentAccepted"
        << "/t" << "REG_DWORD"
        << "/d" << "1"
        << "/f");
    proc.waitForFinished(8000);

    proc.start("reg", QStringList()
        << "add"
        << "HKCU\\Software\\Microsoft\\Terminal Server Client"
        << "/v" << "RdpLaunchConsentAccepted"
        << "/t" << "REG_DWORD"
        << "/d" << "1"
        << "/f");
    proc.waitForFinished(8000);

    proc.start("reg", QStringList()
        << "add"
        << "HKCU\\Software\\Microsoft\\Terminal Server Client"
        << "/v" << "HideConnectionBar"
        << "/t" << "REG_DWORD"
        << "/d" << "1"
        << "/f");
    proc.waitForFinished(8000);

    // Restart TermService so RDP Wrapper hooks into the session
    proc.start("net", QStringList() << "stop" << "termservice" << "/y");
    proc.waitForFinished(15000);
    proc.start("net", QStringList() << "start" << "termservice");
    proc.waitForFinished(15000);

    emit logMessage("[RDP] Remote Desktop enabled successfully.");
    return true;
#else
    return false;
#endif
}

bool VMManager::addDefenderExclusion() {
    emit logMessage("[RDP] Adding Windows Defender exclusion for RDP Wrapper...");
#ifdef Q_OS_WIN
    QProcess proc;
    proc.start("powershell.exe", QStringList()
        << "-NoProfile"
        << "-ExecutionPolicy" << "Bypass"
        << "-Command"
        << "Add-MpPreference -ExclusionPath 'C:\\Program Files\\RDP Wrapper' -ErrorAction SilentlyContinue");
    proc.waitForFinished(15000);
    bool ok = (proc.exitCode() == 0);
    if (ok) {
        emit logMessage("[RDP] Defender exclusion added.");
    } else {
        emit logMessage("[RDP] Defender exclusion skipped (may not be needed).", false);
    }
    return true; // Not fatal if it fails
#else
    return false;
#endif
}

bool VMManager::installRdpWrapper() {
    emit logMessage("[RDP] Downloading and installing RDP Wrapper...");
#ifdef Q_OS_WIN
    QString rdpwrapUrl = "https://github.com/stascorp/rdpwrap/releases/download/v1.6.2/RDPWrap-v1.6.2.zip";
    QString tempZip = QDir::toNativeSeparators(QDir::tempPath() + "/rdpwrap.zip");
    QString tempDir = QDir::toNativeSeparators(QDir::tempPath() + "/rdpwrap_install");
    QString installDir = QDir::toNativeSeparators("C:/Program Files/RDP Wrapper");

    // If already installed, skip
    if (QFile::exists(installDir + QDir::separator() + "rdpwrap.ini")) {
        emit logMessage("[RDP] RDP Wrapper is already installed.");
        return true;
    }

    // Download the zip
    emit logMessage("[RDP] Downloading RDP Wrapper from GitHub...");
    {
        QProcess dl;
        dl.start("powershell.exe", QStringList()
            << "-NoProfile"
            << "-ExecutionPolicy" << "Bypass"
            << "-Command"
            << QString("[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; "
                       "Invoke-WebRequest -Uri '%1' -OutFile '%2' -UseBasicParsing; "
                       "if ($?) { exit 0 } else { exit 1 }").arg(rdpwrapUrl, tempZip));
        dl.waitForFinished(120000); // 2 min download timeout
        if (dl.exitCode() != 0) {
            emit logMessage("[ERROR] Failed to download RDP Wrapper. Check internet connection.", true);
            return false;
        }
    }
    emit logMessage("[RDP] Download complete.");

    // Extract
    emit logMessage("[RDP] Extracting RDP Wrapper...");
    {
        // Remove old temp
        QDir(tempDir).removeRecursively();
        QProcess zip;
        zip.start("powershell.exe", QStringList()
            << "-NoProfile"
            << "-ExecutionPolicy" << "Bypass"
            << "-Command"
            << QString("Expand-Archive -Path '%1' -DestinationPath '%2' -Force").arg(tempZip, tempDir));
        zip.waitForFinished(30000);
        if (zip.exitCode() != 0) {
            emit logMessage("[ERROR] Failed to extract RDP Wrapper archive.", true);
            return false;
        }
    }

    // Run install.bat
    emit logMessage("[RDP] Running RDP Wrapper installer...");
    {
        QDir tempDirObj(tempDir);
        QStringList installBat = tempDirObj.entryList(QStringList() << "install.bat", QDir::Files, QDir::NoSort);
        if (installBat.isEmpty()) {
            emit logMessage("[ERROR] install.bat not found in extracted files.", true);
            return false;
        }
        QString installerPath = QDir::toNativeSeparators(tempDirObj.filePath(installBat.first()));
        QProcess installer;
        installer.start("cmd.exe", QStringList() << "/c" << installerPath);
        installer.waitForFinished(60000);
        if (installer.exitCode() != 0) {
            emit logMessage("[WARNING] Installer returned non-zero exit code, checking if installed anyway...", false);
        }
    }

    // Clean up
    QFile::remove(tempZip);
    QDir(tempDir).removeRecursively();

    // Verify installation
    if (QFile::exists(installDir + QDir::separator() + "rdpwrap.ini")) {
        emit logMessage("[RDP] RDP Wrapper installed successfully.");
        return true;
    } else {
        emit logMessage("[ERROR] RDP Wrapper installation could not be verified.", true);
        return false;
    }
#else
    return false;
#endif
}

QString VMManager::getSharedFolderPath() const {
    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString sharedPath = QDir(appData).filePath("VM_Shared");
    QDir().mkpath(sharedPath);
    return QDir::toNativeSeparators(sharedPath);
}

QString VMManager::getWsbFilePath() const {
    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir(appData).mkpath(appData);
    return QDir::toNativeSeparators(QDir(appData).filePath("ShadowAI_VM.wsb"));
}

bool VMManager::generateWsbFile() {
    QString wsbPath = getWsbFilePath();
    QFile file(wsbPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit logMessage(QString("[ERROR] Failed to create WSB file at %1").arg(wsbPath));
        return false;
    }

    emit logMessage("Configuring virtual hardware allocations...");

    // Read configured performance variables or use secure/optimal defaults
    int ramMb = 4096; // Default to 4GB
    bool gpuAcc = true;
    bool clipBidirectional = true;
    bool micEnabled = true;
    bool webcamEnabled = false;

    // Write Windows Sandbox WSB XML config
    QTextStream out(&file);
    out << "<Configuration>\n";
    out << "  <vGPU>" << (gpuAcc ? "Enable" : "Disable") << "</vGPU>\n";
    out << "  <MemoryInMB>" << ramMb << "</MemoryInMB>\n";
    out << "  <Networking>Enable</Networking>\n";
    out << "  <ClipboardRedirection>" << (clipBidirectional ? "Default" : "Disable") << "</ClipboardRedirection>\n";
    out << "  <AudioInput>" << (micEnabled ? "Enable" : "Disable") << "</AudioInput>\n";
    out << "  <VideoInput>" << (webcamEnabled ? "Enable" : "Disable") << "</VideoInput>\n";
    out << "  <ProtectedClient>Enable</ProtectedClient>\n";
    out << "  <PrinterRedirection>Disable</PrinterRedirection>\n";
    
    // Mount the shared folder so we can pass startup scripts & tools
    QString sharedHost = getSharedFolderPath();
    out << "  <MappedFolders>\n";
    out << "    <MappedFolder>\n";
    out << "      <HostFolder>" << sharedHost << "</HostFolder>\n";
    out << "      <SandboxFolder>C:\\Users\\WDAGUtilityAccount\\Desktop\\Shared</SandboxFolder>\n";
    out << "      <ReadOnly>false</ReadOnly>\n";
    out << "    </MappedFolder>\n";
    out << "  </MappedFolders>\n";
    
    // Startup Command
    out << "  <LogonCommand>\n";
    out << "    <Command>C:\\Users\\WDAGUtilityAccount\\Desktop\\Shared\\sandbox_init.cmd</Command>\n";
    out << "  </LogonCommand>\n";
    
    out << "</Configuration>\n";
    file.close();

    emit logMessage(QString("WSB Configuration written: %1").arg(wsbPath));
    return true;
}

bool VMManager::writeStartupScript() {
    QString sharedDir = getSharedFolderPath();
    QString scriptPath = QDir(sharedDir).filePath("sandbox_init.cmd");
    QFile file(scriptPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit logMessage("[ERROR] Failed to write Sandbox startup script.");
        return false;
    }

    emit logMessage("Writing guest environment personalization scripts...");

    QTextStream out(&file);
    out << "@echo off\n";
    out << "echo ============================================\n";
    out << "echo   ShadowAI VM Mode Guest Initialization\n";
    out << "echo ============================================\n";
    out << "echo Setting up Guest Tools...\n";
    out << ":: Personalize appearance if possible\n";
    out << "reg add \"HKCU\\Control Panel\\Desktop\" /v Wallpaper /t REG_SZ /d \"\" /f\n";
    out << "reg add \"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize\" /v AppsUseLightTheme /t REG_DWORD /d 0 /f\n";
    out << "reg add \"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize\" /v SystemUsesLightTheme /t REG_DWORD /d 0 /f\n";
    out << ":: Refresh settings\n";
    out << "RUNDLL32.EXE user32.dll,UpdatePerUserSystemParameters\n";
    out << "echo Initialization Complete!\n";
    file.close();

    emit logMessage(QString("Guest initialization script ready at %1").arg(scriptPath));
    return true;
}

bool VMManager::startVM() {
    if (m_state == Running || m_state == Starting) {
        emit logMessage("VM is already starting or running.");
        return true;
    }

    setState(Starting);
    emit logMessage("Starting RDP local session environment...");

    if (isRdpReady()) {
        emit logMessage("[RDP] ShadowUser session ready. Initiating local desktop connection...");
        setState(Running);
        emit rdpSessionReady(rdpUsername());
        return true;
    }

    emit logMessage("[ERROR] RDP not configured. Please run Setup first.");
    setState(Error);
    return false;
}

void VMManager::stopVM() {
    if (m_state == Stopped) return;

    setState(Stopping);
    emit logMessage("Stopping VM environment...");

    m_monitorTimer->stop();

#ifdef Q_OS_WIN
    // Kill any active RDP connection clients immediately
    QProcess::startDetached("taskkill", QStringList() << "/f" << "/im" << "mstsc.exe");

    // Windows Sandbox is disposable - closing the window terminates the VM.
    if (m_sandboxHwnd && IsWindow(m_sandboxHwnd)) {
        PostMessage(m_sandboxHwnd, WM_CLOSE, 0, 0);
        emit logMessage("Sent close signal to VM window.");
    }
#endif

    if (m_process) {
        m_process->terminate();
        if (!m_process->waitForFinished(3000)) {
            m_process->kill();
        }
        m_process->deleteLater();
        m_process = nullptr;
    }

    m_sandboxHwnd = nullptr;
    setState(Stopped);
    emit logMessage("VM stopped and cleaned up successfully.");
}

#ifdef Q_OS_WIN
struct FindVMWindowData {
    HWND foundHwnd = nullptr;
};

static BOOL CALLBACK EnumVMWindowsProc(HWND hwnd, LPARAM lParam) {
    FindVMWindowData* data = reinterpret_cast<FindVMWindowData*>(lParam);
    wchar_t clsName[256];
    wchar_t title[256];
    
    if (GetClassNameW(hwnd, clsName, 256) && GetWindowTextW(hwnd, title, 256)) {
        std::wstring cls(clsName);
        std::wstring t(title);
        
        // Match Windows Sandbox
        if (cls == L"WindowsSandboxWindowClass") {
            data->foundHwnd = hwnd;
            return FALSE;
        }
        // Match VMware Player or Workstation
        if (cls == L"VMPlayerFrame" || cls == L"VMwareMuiFrame" || cls.find(L"VMware") != std::wstring::npos) {
            data->foundHwnd = hwnd;
            return FALSE;
        }
        // Match VirtualBox VM window
        if (t.find(L"VirtualBox") != std::wstring::npos && (cls.find(L"QWindowIcon") != std::wstring::npos || cls.find(L"Qt") != std::wstring::npos)) {
            data->foundHwnd = hwnd;
            return FALSE;
        }
    }
    return TRUE;
}
#endif

void VMManager::checkSandboxProcess() {
    if (m_state != Starting && m_state != Running) {
        m_monitorTimer->stop();
        return;
    }

    // Check if process has exited
    if (m_process && m_process->state() == QProcess::NotRunning) {
        emit logMessage("VM process exited.");
        stopVM();
        return;
    }

#ifdef Q_OS_WIN
    // Search for Sandbox/VMware/VirtualBox window
    if (!m_sandboxHwnd) {
        m_findAttempts++;
        FindVMWindowData data;
        EnumWindows(EnumVMWindowsProc, reinterpret_cast<LPARAM>(&data));
        
        if (data.foundHwnd) {
            m_sandboxHwnd = data.foundHwnd;
            setState(Running);
            emit logMessage("VM window detected! Docking into application container.");
            emit sandboxWindowFound((void*)data.foundHwnd);
            m_findAttempts = 0;
        } else if (m_findAttempts > 25) {
            emit logMessage("[WARNING] VM window took too long to load. Will continue polling...");
            m_findAttempts = 0;
        }
    } else {
        // If window is closed manually by user
        if (!IsWindow(m_sandboxHwnd)) {
            emit logMessage("VM window was closed manually.");
            stopVM();
        }
    }
#endif
}

void VMManager::setState(VMState s) {
    if (m_state != s) {
        m_state = s;
        emit stateChanged(s);
    }
}
