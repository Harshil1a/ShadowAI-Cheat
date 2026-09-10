#include "hotkeymanager.h"
#include "appconfig.h"
#include <QSettings>

#ifdef Q_OS_WIN
#include <windows.h>

static HotkeyManager* s_instance = nullptr;
static HHOOK s_hook = nullptr;

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && s_instance) {
        // Bypass host hotkeys hook completely if focused on the RDP session window
        char className[256] = {0};
        HWND activeHwnd = GetForegroundWindow();
        if (activeHwnd) {
            GetClassNameA(activeHwnd, className, sizeof(className));
            if (strcmp(className, "TscShellContainerClass") == 0 || strcmp(className, "UIMainClass") == 0) {
                return CallNextHookEx(nullptr, nCode, wParam, lParam);
            }
        }

        KBDLLHOOKSTRUCT* pKey = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        
        // Intercept keys on down and up actions
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            bool ctrlPressed  = GetAsyncKeyState(VK_CONTROL) & 0x8000;
            bool shiftPressed = GetAsyncKeyState(VK_SHIFT) & 0x8000;
            bool altPressed   = GetAsyncKeyState(VK_MENU) & 0x8000;

            // ─── EMERGENCY PANIC KILL-SWITCH ───
            // Default Ctrl+Shift+Del or Ctrl+Alt+Del alternative configured in settings
            int panicVk = AppConfig::instance().hotkeyPanic();
            if ((ctrlPressed && shiftPressed && pKey->vkCode == panicVk) ||
                (ctrlPressed && altPressed && pKey->vkCode == panicVk)) {
                emit s_instance->panicTriggered();
                return 1;
            }

            // Standard Shortcuts (Shift + Alt + Key)
            if (altPressed && shiftPressed) {
                int vk = pKey->vkCode;
                // If it's one of our registered hotkeys, handle it and swallow the input
                if (s_instance->handleHookKey(vk, true)) {
                    return 1; // 1 = swallow this input
                }
            }
        } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
            int vk = pKey->vkCode;
            // Let release pass to handleHookKey with isDown = false
            if (s_instance->handleHookKey(vk, false)) {
                return 1;
            }
        }
    }
    // Pass other inputs to the next hook
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}
#endif

HotkeyManager::HotkeyManager(QObject* parent)
    : QObject(parent)
{
}

HotkeyManager::~HotkeyManager() {
    unregisterAll();
}

void HotkeyManager::registerAll() {
#ifdef Q_OS_WIN
    s_instance = this;
    if (!s_hook) {
        s_hook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(nullptr), 0);
    }
#endif
}

void HotkeyManager::unregisterAll() {
#ifdef Q_OS_WIN
    if (s_hook) {
        UnhookWindowsHookEx(s_hook);
        s_hook = nullptr;
        // Debug output removed for stealth
    }
    s_instance = nullptr;
#endif
}

void HotkeyManager::reregisterAll() {
    unregisterAll();
    registerAll();
}

bool HotkeyManager::handleHookKey(int vk, bool isDown) {
    auto& cfg = AppConfig::instance();
    
    if (isDown) {
        if (vk == cfg.hotkeyToggle()) {
            emit toggleOverlay();
            return true;
        }
        if (vk == cfg.hotkeyScreenshot()) {
            emit takeScreenshot();
            return true;
        }
        if (vk == cfg.hotkeyGetAnswer()) {
            emit getAnswer();
            return true;
        }
        if (vk == cfg.hotkeyScrollUp()) {
            emit scrollUp();
            return true;
        }
        if (vk == cfg.hotkeyScrollDown()) {
            emit scrollDown();
            return true;
        }
        if (vk == cfg.hotkeyTransparency()) {
            emit cycleTransparency();
            return true;
        }
        if (vk == cfg.hotkeyClear()) {
            emit clearAnswer();
            return true;
        }
        if (vk == cfg.hotkeyMoveLeft()) {
            emit moveLeft();
            return true;
        }
        if (vk == cfg.hotkeyMoveRight()) {
            emit moveRight();
            return true;
        }
        if (vk == cfg.hotkeyMoveUp()) {
            emit moveUp();
            return true;
        }
        if (vk == cfg.hotkeyMoveDown()) {
            emit moveDown();
            return true;
        }
        if (vk == cfg.hotkeyVoice()) {
            emit voiceRecordDown();
            return true;
        }
        if (vk == cfg.hotkeyToggleBadges()) {
            emit toggleBadges();
            return true;
        }
        if (vk == cfg.hotkeyCopyScreenshot()) {
            emit copyScreenshot();
            return true;
        }
        if (vk == cfg.hotkeyGhostWriter()) {
            emit ghostWriter();
            return true;
        }

    } else {
        // Key UP (Release) events
        if (vk == cfg.hotkeyVoice()) {
            emit voiceRecordUp();
            return true;
        }
    }
    
    return false;
}
