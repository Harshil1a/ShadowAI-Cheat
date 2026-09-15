#include "hotkeymanager.h"
#include "appconfig.h"
#include <QSettings>

// WINDOWS: Low-Level Keyboard Hook
#ifdef Q_OS_WIN
#include <windows.h>

static HotkeyManager* s_instance = nullptr;
static HHOOK s_hook = nullptr;

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && s_instance) {
        char className[256] = {0};
        HWND activeHwnd = GetForegroundWindow();
        if (activeHwnd) {
            GetClassNameA(activeHwnd, className, sizeof(className));
            if (strcmp(className, "TscShellContainerClass") == 0 || strcmp(className, "UIMainClass") == 0) {
                return CallNextHookEx(nullptr, nCode, wParam, lParam);
            }
        }
        KBDLLHOOKSTRUCT* pKey = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            bool ctrlPressed  = GetAsyncKeyState(VK_CONTROL) & 0x8000;
            bool shiftPressed = GetAsyncKeyState(VK_SHIFT) & 0x8000;
            bool altPressed   = GetAsyncKeyState(VK_MENU) & 0x8000;
            int panicVk = AppConfig::instance().hotkeyPanic();
            if ((ctrlPressed && shiftPressed && pKey->vkCode == (unsigned)panicVk) ||
                (ctrlPressed && altPressed && pKey->vkCode == (unsigned)panicVk)) {
                emit s_instance->panicTriggered();
                return 1;
            }
            if (altPressed && shiftPressed) {
                int vk = pKey->vkCode;
                if (s_instance->handleHookKey(vk, true)) return 1;
            }
        } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
            int vk = pKey->vkCode;
            if (s_instance->handleHookKey(vk, false)) return 1;
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}
#endif // Q_OS_WIN

// macOS: Carbon Event HotKeys (Shift+Option+Key)
// App name on Mac = "AudioService" (set in CMakeLists) - looks innocent
#if defined(Q_OS_MAC) || defined(Q_OS_MACOS)
#include <Carbon/Carbon.h>
#include <vector>
#include <set>

static HotkeyManager* s_macInstance = nullptr;
static EventHandlerRef s_macEventHandler = nullptr;

struct MacRegisteredHotKey {
    int vk;
    EventHotKeyRef ref;
};
static std::vector<MacRegisteredHotKey> s_macHotkeys;

// Map Qt VK code to macOS Carbon key code
static int qtVkToMacKeyCode(int vk) {
    switch (vk) {
        case 0x41: return kVK_ANSI_A; case 0x42: return kVK_ANSI_B;
        case 0x43: return kVK_ANSI_C; case 0x44: return kVK_ANSI_D;
        case 0x45: return kVK_ANSI_E; case 0x46: return kVK_ANSI_F;
        case 0x47: return kVK_ANSI_G; case 0x48: return kVK_ANSI_H;
        case 0x49: return kVK_ANSI_I; case 0x4A: return kVK_ANSI_J;
        case 0x4B: return kVK_ANSI_K; case 0x4C: return kVK_ANSI_L;
        case 0x4D: return kVK_ANSI_M; case 0x4E: return kVK_ANSI_N;
        case 0x4F: return kVK_ANSI_O; case 0x50: return kVK_ANSI_P;
        case 0x51: return kVK_ANSI_Q; case 0x52: return kVK_ANSI_R;
        case 0x53: return kVK_ANSI_S; case 0x54: return kVK_ANSI_T;
        case 0x55: return kVK_ANSI_U; case 0x56: return kVK_ANSI_V;
        case 0x57: return kVK_ANSI_W; case 0x58: return kVK_ANSI_X;
        case 0x59: return kVK_ANSI_Y; case 0x5A: return kVK_ANSI_Z;
        case 0x30: return kVK_ANSI_0; case 0x31: return kVK_ANSI_1;
        case 0x32: return kVK_ANSI_2; case 0x33: return kVK_ANSI_3;
        case 0x34: return kVK_ANSI_4; case 0x35: return kVK_ANSI_5;
        case 0x36: return kVK_ANSI_6; case 0x37: return kVK_ANSI_7;
        case 0x38: return kVK_ANSI_8; case 0x39: return kVK_ANSI_9;
        case 0x70: return kVK_F1;  case 0x71: return kVK_F2;
        case 0x72: return kVK_F3;  case 0x73: return kVK_F4;
        case 0x74: return kVK_F5;  case 0x75: return kVK_F6;
        case 0x76: return kVK_F7;  case 0x77: return kVK_F8;
        case 0x78: return kVK_F9;  case 0x79: return kVK_F10;
        case 0x7A: return kVK_F11; case 0x7B: return kVK_F12;
        case 0x25: return kVK_LeftArrow;  case 0x27: return kVK_RightArrow;
        case 0x26: return kVK_UpArrow;    case 0x28: return kVK_DownArrow;
        case 0x2E: return kVK_ForwardDelete;
        case 0x20: return kVK_Space;
        case 0x0D: return kVK_Return;
        case 0x09: return kVK_Tab;
        case 0x1B: return kVK_Escape;
        case 0x24: return kVK_Home;
        case 0x23: return kVK_End;
        case 0x21: return kVK_PageUp;
        case 0x22: return kVK_PageDown;
        default:   return -1;
    }
}

static OSStatus macHotKeyHandler(EventHandlerCallRef nextHandler, EventRef theEvent, void* userData) {
    Q_UNUSED(nextHandler);
    Q_UNUSED(userData);
    if (!s_macInstance) return noErr;

    EventHotKeyID hkId;
    OSStatus status = GetEventParameter(theEvent, kEventParamDirectObject, typeEventHotKeyID, NULL, sizeof(hkId), NULL, &hkId);
    if (status != noErr) return noErr;

    UInt32 eventKind = GetEventKind(theEvent);
    if (eventKind == kEventHotKeyPressed) {
        if (hkId.id == 99999) {
            emit s_macInstance->panicTriggered();
            return noErr;
        }
        s_macInstance->handleHookKey((int)hkId.id, true);
    } else if (eventKind == kEventHotKeyReleased) {
        if (hkId.id != 99999) {
            s_macInstance->handleHookKey((int)hkId.id, false);
        }
    }
    return noErr;
}
#endif // Q_OS_MAC

// ---- HotkeyManager methods ----

HotkeyManager::HotkeyManager(QObject* parent) : QObject(parent) {}

HotkeyManager::~HotkeyManager() { unregisterAll(); }

void HotkeyManager::registerAll() {
#ifdef Q_OS_WIN
    s_instance = this;
    if (!s_hook)
        s_hook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(nullptr), 0);
#endif
#if defined(Q_OS_MAC) || defined(Q_OS_MACOS)
    s_macInstance = this;

    // 1. Install Carbon event handler if not already installed
    if (!s_macEventHandler) {
        EventTypeSpec eventSpecs[2];
        eventSpecs[0].eventClass = kEventClassKeyboard;
        eventSpecs[0].eventKind = kEventHotKeyPressed;
        eventSpecs[1].eventClass = kEventClassKeyboard;
        eventSpecs[1].eventKind = kEventHotKeyReleased;
        InstallApplicationEventHandler(NewEventHandlerUPP(macHotKeyHandler), 2, eventSpecs, NULL, &s_macEventHandler);
    }

    // 2. Unregister any existing hotkeys first to avoid duplicates
    for (const auto& hk : s_macHotkeys) {
        if (hk.ref) {
            UnregisterEventHotKey(hk.ref);
        }
    }
    s_macHotkeys.clear();

    auto& cfg = AppConfig::instance();
    std::set<int> registeredVks;

    auto regKey = [&](int vk) {
        if (vk <= 0 || registeredVks.count(vk)) return;
        int macKc = qtVkToMacKeyCode(vk);
        if (macKc < 0) return;

        EventHotKeyID hkId;
        hkId.signature = 'SHAD';
        hkId.id = (UInt32)vk;
        EventHotKeyRef ref = nullptr;
        UInt32 mods = (UInt32)(shiftKey | optionKey);
        OSStatus err = RegisterEventHotKey((UInt32)macKc, mods, hkId, GetApplicationEventTarget(), 0, &ref);
        if (err == noErr && ref) {
            s_macHotkeys.push_back({vk, ref});
            registeredVks.insert(vk);
        }
    };

    regKey(cfg.hotkeyToggle());
    regKey(cfg.hotkeyScreenshot());
    regKey(cfg.hotkeyGetAnswer());
    regKey(cfg.hotkeyScrollUp());
    regKey(cfg.hotkeyScrollDown());
    regKey(cfg.hotkeyTransparency());
    regKey(cfg.hotkeyClear());
    regKey(cfg.hotkeyMoveLeft());
    regKey(cfg.hotkeyMoveRight());
    regKey(cfg.hotkeyMoveUp());
    regKey(cfg.hotkeyMoveDown());
    regKey(cfg.hotkeyVoice());
    regKey(cfg.hotkeyToggleBadges());
    regKey(cfg.hotkeyCopyScreenshot());
    regKey(cfg.hotkeyGhostWriter());
    regKey(cfg.hotkeyHideStrip());

    // Register Panic Hotkey (Cmd + Shift + Key)
    int panicVk = cfg.hotkeyPanic();
    if (panicVk > 0) {
        int panicMacKc = qtVkToMacKeyCode(panicVk);
        if (panicMacKc >= 0) {
            EventHotKeyID hkId;
            hkId.signature = 'SHAD';
            hkId.id = 99999;
            EventHotKeyRef ref = nullptr;
            UInt32 panicMods = (UInt32)(cmdKey | shiftKey);
            OSStatus err = RegisterEventHotKey((UInt32)panicMacKc, panicMods, hkId, GetApplicationEventTarget(), 0, &ref);
            if (err == noErr && ref) {
                s_macHotkeys.push_back({panicVk, ref});
            }
        }
    }
#endif
}

void HotkeyManager::unregisterAll() {
#ifdef Q_OS_WIN
    if (s_hook) { UnhookWindowsHookEx(s_hook); s_hook = nullptr; }
    s_instance = nullptr;
#endif
#if defined(Q_OS_MAC) || defined(Q_OS_MACOS)
    for (const auto& hk : s_macHotkeys) {
        if (hk.ref) {
            UnregisterEventHotKey(hk.ref);
        }
    }
    s_macHotkeys.clear();
    if (s_macEventHandler) {
        RemoveEventHandler(s_macEventHandler);
        s_macEventHandler = nullptr;
    }
    s_macInstance = nullptr;
#endif
}

void HotkeyManager::reregisterAll() { unregisterAll(); registerAll(); }

bool HotkeyManager::handleHookKey(int vk, bool isDown) {
    auto& cfg = AppConfig::instance();
    if (isDown) {
        if (vk == cfg.hotkeyToggle())        { emit toggleOverlay();     return true; }
        if (vk == cfg.hotkeyScreenshot())    { emit takeScreenshot();    return true; }
        if (vk == cfg.hotkeyGetAnswer())     { emit getAnswer();          return true; }
        if (vk == cfg.hotkeyScrollUp())      { emit scrollUp();           return true; }
        if (vk == cfg.hotkeyScrollDown())    { emit scrollDown();         return true; }
        if (vk == cfg.hotkeyTransparency())  { emit cycleTransparency();  return true; }
        if (vk == cfg.hotkeyClear())         { emit clearAnswer();        return true; }
        if (vk == cfg.hotkeyMoveLeft())      { emit moveLeft();           return true; }
        if (vk == cfg.hotkeyMoveRight())     { emit moveRight();          return true; }
        if (vk == cfg.hotkeyMoveUp())        { emit moveUp();             return true; }
        if (vk == cfg.hotkeyMoveDown())      { emit moveDown();           return true; }
        if (vk == cfg.hotkeyVoice())         { emit voiceRecordDown();    return true; }
        if (vk == cfg.hotkeyToggleBadges())  { emit toggleBadges();       return true; }
        if (vk == cfg.hotkeyCopyScreenshot()){ emit copyScreenshot();     return true; }
        if (vk == cfg.hotkeyGhostWriter())   { emit ghostWriter();        return true; }
        if (vk == cfg.hotkeyHideStrip())     { emit hideStrip();          return true; }
    } else {
        if (vk == cfg.hotkeyVoice()) { emit voiceRecordUp(); return true; }
    }
    return false;
}
