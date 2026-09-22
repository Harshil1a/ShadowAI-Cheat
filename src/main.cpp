#include <QApplication>
#include <QClipboard>
#include <QSystemTrayIcon>
#include <QMessageBox>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <cstdlib>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#if defined(Q_OS_MAC) || defined(Q_OS_MACOS)
#include <CoreGraphics/CoreGraphics.h>
#endif

#include "overlaywindow.h"
#include "settingswindow.h"
#include "hotkeymanager.h"
#include "screencapture.h"
#include "aimanager.h"
#include "trayicon.h"
#include "appconfig.h"
#include "dashboard.h"
#include "mainwindow.h"
#include "accountmanager.h"



int main(int argc, char* argv[]) {
#ifdef Q_OS_WIN
    HANDLE hMutex = CreateMutexA(NULL, TRUE, "ShadowAI_SingleInstance_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    SetConsoleTitleA("Runtime Broker");
    SetProcessDPIAware();
#endif

    QApplication app(argc, argv);
    app.setApplicationName("ShadowAI");
    app.setOrganizationName("ShadowAI");
    app.setApplicationDisplayName("Runtime Broker");
    app.setQuitOnLastWindowClosed(false);

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        QMessageBox::critical(nullptr, "System Broker",
            "System components not available on this system.");
        return 1;
    }

    AppConfig::instance().load();

#if defined(Q_OS_MAC) || defined(Q_OS_MACOS)
    // Request screen recording permission once on startup if not already authorized
    if (!CGPreflightScreenCaptureAccess()) {
        CGRequestScreenCaptureAccess();
    }
#endif

    auto* aiManager    = new AIManager(&app);
    auto* screenCap    = new ScreenCapture(&app);
    auto* hotkeyMgr    = new HotkeyManager(&app);
    auto* overlay      = new OverlayWindow();
    auto* tray         = new TrayIcon(&app);
    auto* mainWindow   = new MainWindow();
    auto* dashboard    = mainWindow->dashboard();

    overlay->setAIManager(aiManager);
    overlay->setScreenCapture(screenCap);

    // ── Helper: Compulsory Google Login Gate ────────────────────────────────
    auto requireLogin = [mainWindow, dashboard]() -> bool {
        if (!AccountManager::instance().isLoggedIn()) {
            mainWindow->show();
            mainWindow->raise();
            mainWindow->activateWindow();
            dashboard->updateStatus(false, false);
            AccountManager::instance().startGoogleLogin();
            return false;
        }
        return true;
    };

    // ── Connect hotkeys to overlay ────────────────────────────────────────────
    QObject::connect(hotkeyMgr, &HotkeyManager::toggleOverlay, overlay, [overlay, dashboard, requireLogin]() {
        if (!requireLogin()) return;
        overlay->toggleVisibility();
        dashboard->updateStatus(true, overlay->isVisible());
    });

    QObject::connect(hotkeyMgr, &HotkeyManager::takeScreenshot,
                     overlay,   [overlay, requireLogin]() {
        if (!requireLogin()) return;
        overlay->doScreenshot();
    });

    QObject::connect(hotkeyMgr, &HotkeyManager::getAnswer,
                     overlay,   [overlay, requireLogin]() {
        if (!requireLogin()) return;
        overlay->doGetAnswer();
    });

    QObject::connect(hotkeyMgr, &HotkeyManager::scrollUp,
                     overlay,   &OverlayWindow::scrollContentUp);

    QObject::connect(hotkeyMgr, &HotkeyManager::scrollDown,
                     overlay,   &OverlayWindow::scrollContentDown);

    QObject::connect(hotkeyMgr, &HotkeyManager::cycleTransparency,
                     overlay,   &OverlayWindow::cycleTransparency);

    QObject::connect(hotkeyMgr, &HotkeyManager::clearAnswer,
                     overlay,   &OverlayWindow::clearAll);

    QObject::connect(hotkeyMgr, &HotkeyManager::moveLeft,
                     overlay,   &OverlayWindow::moveLeft);

    QObject::connect(hotkeyMgr, &HotkeyManager::moveRight,
                     overlay,   &OverlayWindow::moveRight);

    QObject::connect(hotkeyMgr, &HotkeyManager::moveUp,
                     overlay,   &OverlayWindow::moveUp);

    QObject::connect(hotkeyMgr, &HotkeyManager::moveDown,
                     overlay,   &OverlayWindow::moveDown);

    QObject::connect(hotkeyMgr, &HotkeyManager::voiceRecordDown,
                     overlay,   &OverlayWindow::toggleVoiceRecord);

    QObject::connect(hotkeyMgr, &HotkeyManager::voiceRecordUp,
                     overlay,   &OverlayWindow::stopVoiceRecordOnRelease);

    QObject::connect(hotkeyMgr, &HotkeyManager::toggleBadges,
                     overlay,   &OverlayWindow::toggleBadgesVisibility);

    QObject::connect(hotkeyMgr, &HotkeyManager::hideStrip,
                     overlay,   &OverlayWindow::toggleHideStrip);

    QObject::connect(hotkeyMgr, &HotkeyManager::copyScreenshot,
                     overlay,   &OverlayWindow::copyScreenshotToClipboard);

    QObject::connect(hotkeyMgr, &HotkeyManager::ghostWriter,
                     overlay,   [overlay, requireLogin]() {
        if (!requireLogin()) return;
        overlay->doGhostWriter();
    });

    // ── Connect tray ──────────────────────────────────────────────────────────
    QObject::connect(tray, &TrayIcon::toggleOverlay, overlay, [overlay, dashboard, requireLogin]() {
        if (!requireLogin()) return;
        overlay->toggleVisibility();
        dashboard->updateStatus(true, overlay->isVisible());
    });

    QObject::connect(tray, &TrayIcon::showDashboard, mainWindow, &MainWindow::show);

    QObject::connect(tray, &TrayIcon::openSettings, mainWindow, [mainWindow, dashboard]() {
        mainWindow->show();
        mainWindow->raise();
        mainWindow->activateWindow();
        dashboard->openSettingsPage();
    });

    QObject::connect(dashboard->settingsWidget(), &SettingsWindow::settingsSaved, hotkeyMgr, [hotkeyMgr, aiManager, overlay]() {
        // Re-register hotkeys with new keys
        hotkeyMgr->reregisterAll();
        // Reload API keys
        aiManager->loadKeys();
        // Refresh overlay size/settings
        overlay->refreshSettings();
    });

    // ── Emergency Panic Kill-Switch ───────────────────────────────────────────
    QObject::connect(hotkeyMgr, &HotkeyManager::panicTriggered, [overlay, mainWindow]() {
        QClipboard* cb = QGuiApplication::clipboard();
        if (cb) cb->clear();
#ifdef Q_OS_WIN
        if (OpenClipboard(nullptr)) {
            EmptyClipboard();
            CloseClipboard();
        }
#endif
        overlay->hide();
        mainWindow->hide();
#ifdef Q_OS_WIN
        TerminateProcess(GetCurrentProcess(), 0);
#else
        std::exit(0);
#endif
    });

    QObject::connect(tray, &TrayIcon::quit, &app, [overlay, hotkeyMgr, mainWindow]() {
        AppConfig::instance().setOverlayPos(overlay->x(), overlay->y());
        AppConfig::instance().save();
        hotkeyMgr->unregisterAll();
        mainWindow->hide();
        overlay->stopAll();
        qApp->quit();
    });

    // ── Dashboard connections ────────────────────────────────────────────────
    // Unified state management for Start/Stop Assistant
    QObject::connect(dashboard, &Dashboard::toggleOverlay, overlay, [overlay, hotkeyMgr, dashboard, requireLogin]() {
        if (!requireLogin()) return;
        if (dashboard->isOverlayRunning()) {
            // Stop Assistant (hide overlay)
            overlay->hide();
            dashboard->updateStatus(false, false);
        } else {
            // Start Assistant (show overlay and ensure hotkeys are active)
            overlay->toggleVisibility(); // show overlay
            hotkeyMgr->registerAll();
            dashboard->updateStatus(true, overlay->isVisible());
        }
    });

    QObject::connect(dashboard, &Dashboard::openSettings, tray, [&]() {
        emit tray->openSettings();
    });

    QObject::connect(dashboard, &Dashboard::quitApp, tray, &TrayIcon::quit);

    QObject::connect(dashboard, &Dashboard::hideOverlay, overlay, &OverlayWindow::hide);




    // ── Set process priority to mimic system idle behavior ────────────────────
#ifdef Q_OS_WIN
    SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
#endif

    // Initialize Dashboard to Stopped state on startup
    dashboard->updateStatus(false);

    // Register global shortcuts immediately so Shift+Option+H (Mac) and Shift+Alt+H (Windows) work
    hotkeyMgr->registerAll();

    // ── Show Tray (Dashboard stays hidden until tray icon is clicked) ────────
    tray->show();

    mainWindow->show();
    mainWindow->raise();
    mainWindow->activateWindow();

    return app.exec();
}
