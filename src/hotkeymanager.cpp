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

// macOS: NSEvent Global Monitor (Shift+Option+Key)
// App name on Mac = "AudioService" (set in CMakeLists) - looks innocent
#if defined(Q_OS_MAC) || defined(Q_OS_MACOS)
#include <objc/objc.h>
#include <objc/runtime.h>
#include <objc/message.h>
#include <Carbon/Carbon.h>

static HotkeyManager* s_macInstance = nullptr;
static id s_macMonitor = nullptr;

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
        default:   return -1;
    }
}

static void macKeyEventCallback(id event) {
    if (!s_macInstance) return;
    typedef unsigned long NSUInteger;
    typedef NSUInteger (*flags_func)(id, SEL);
    typedef unsigned short (*keyCode_func)(id, SEL);
    NSUInteger flags  = ((flags_func)  objc_msgSend)(event, sel_registerName("modifierFlags"));
    unsigned short kc = ((keyCode_func)objc_msgSend)(event, sel_registerName("keyCode"));
    NSUInteger evType = ((flags_func)  objc_msgSend)(event, sel_registerName("type"));

    // NSEventModifierFlagShift=0x20000, Option=0x80000, Cmd=0x100000
    bool shiftDown  = (flags & 0x20000) != 0;
    bool optionDown = (flags & 0x80000) != 0;
    bool cmdDown    = (flags & 0x100000) != 0;
    bool isDown     = (evType == 10); // NSEventTypeKeyDown

    auto& cfg = AppConfig::instance();

    // Shift + Option + Key -> main hotkeys
    if (shiftDown && optionDown) {
        auto check = [&](int qtVk) { return qtVkToMacKeyCode(qtVk) == (int)kc; };
        if (isDown) {
            if      (check(cfg.hotkeyToggle()))        emit s_macInstance->toggleOverlay();
            else if (check(cfg.hotkeyScreenshot()))    emit s_macInstance->takeScreenshot();
            else if (check(cfg.hotkeyGetAnswer()))     emit s_macInstance->getAnswer();
            else if (check(cfg.hotkeyScrollUp()))      emit s_macInstance->scrollUp();
            else if (check(cfg.hotkeyScrollDown()))    emit s_macInstance->scrollDown();
            else if (check(cfg.hotkeyTransparency()))  emit s_macInstance->cycleTransparency();
            else if (check(cfg.hotkeyClear()))         emit s_macInstance->clearAnswer();
            else if (check(cfg.hotkeyMoveLeft()))      emit s_macInstance->moveLeft();
            else if (check(cfg.hotkeyMoveRight()))     emit s_macInstance->moveRight();
            else if (check(cfg.hotkeyMoveUp()))        emit s_macInstance->moveUp();
            else if (check(cfg.hotkeyMoveDown()))      emit s_macInstance->moveDown();
            else if (check(cfg.hotkeyVoice()))         emit s_macInstance->voiceRecordDown();
            else if (check(cfg.hotkeyToggleBadges()))  emit s_macInstance->toggleBadges();
            else if (check(cfg.hotkeyCopyScreenshot()))emit s_macInstance->copyScreenshot();
            else if (check(cfg.hotkeyGhostWriter()))   emit s_macInstance->ghostWriter();
            else if (check(cfg.hotkeyHideStrip()))     emit s_macInstance->hideStrip();
        } else {
            auto check2 = [&](int qtVk) { return qtVkToMacKeyCode(qtVk) == (int)kc; };
            if (check2(cfg.hotkeyVoice())) emit s_macInstance->voiceRecordUp();
        }
    }

    // Cmd + Shift + Key -> panic
    if (cmdDown && shiftDown && isDown) {
        if (qtVkToMacKeyCode(cfg.hotkeyPanic()) == (int)kc)
            emit s_macInstance->panicTriggered();
    }
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
    if (!s_macMonitor) {
        unsigned long long mask = (1ULL << 10) | (1ULL << 11); // keyDown | keyUp
        Class NSEventClass = objc_getClass("NSEvent");
        SEL addSel = sel_registerName("addGlobalMonitorForEventsMatchingMask:handler:");
        typedef void (^EvBlock)(id);
        EvBlock blk = ^(id ev) { macKeyEventCallback(ev); };
        typedef id (*addFn)(Class, SEL, unsigned long long, EvBlock);
        s_macMonitor = ((addFn)objc_msgSend)(NSEventClass, addSel, mask, blk);
        if (s_macMonitor)
            ((id(*)(id,SEL))objc_msgSend)(s_macMonitor, sel_registerName("retain"));
    }
#endif
}

void HotkeyManager::unregisterAll() {
#ifdef Q_OS_WIN
    if (s_hook) { UnhookWindowsHookEx(s_hook); s_hook = nullptr; }
    s_instance = nullptr;
#endif
#if defined(Q_OS_MAC) || defined(Q_OS_MACOS)
    if (s_macMonitor) {
        Class NSEventClass = objc_getClass("NSEvent");
        ((void(*)(Class,SEL,id))objc_msgSend)(NSEventClass, sel_registerName("removeMonitor:"), s_macMonitor);
        ((void(*)(id,SEL))objc_msgSend)(s_macMonitor, sel_registerName("release"));
        s_macMonitor = nullptr;
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
