#include <windows.h>
#include <shellapi.h>
#include <string>
#include <fstream>
#include <algorithm>
#include <vector>
#include <sstream>
#include <sys/stat.h>

std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
}

bool fileExists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

std::string findChromePath() {
    std::vector<std::string> paths = {
        "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe",
        "C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe"
    };

    char localAppData[MAX_PATH];
    if (GetEnvironmentVariableA("LOCALAPPDATA", localAppData, MAX_PATH) > 0) {
        paths.push_back(std::string(localAppData) + "\\Google\\Chrome\\Application\\chrome.exe");
    }

    for (const auto& p : paths) {
        if (fileExists(p)) {
            return p;
        }
    }
    return "chrome.exe"; // Fallback to path lookup
}

void parseShortcut(int& modifiers, int& vk) {
    modifiers = MOD_ALT | MOD_SHIFT;
    vk = 'A';

    std::ifstream file("C:\\Users\\Public\\ShadowAI_config.txt");
    if (!file.is_open()) {
        return;
    }
    std::string s;
    std::getline(file, s);
    file.close();

    if (s.empty()) return;

    std::vector<std::string> tokens;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, '+')) {
        item.erase(std::remove_if(item.begin(), item.end(), isspace), item.end());
        if (!item.empty()) {
            tokens.push_back(toUpper(item));
        }
    }

    if (tokens.empty()) return;

    int parsedModifiers = 0;
    bool hasModifier = false;
    std::string keyToken = "";

    for (const auto& token : tokens) {
        if (token == "CTRL" || token == "CONTROL") {
            parsedModifiers |= MOD_CONTROL;
            hasModifier = true;
        } else if (token == "ALT") {
            parsedModifiers |= MOD_ALT;
            hasModifier = true;
        } else if (token == "SHIFT") {
            parsedModifiers |= MOD_SHIFT;
            hasModifier = true;
        } else if (token == "WIN" || token == "WINDOWS") {
            parsedModifiers |= MOD_WIN;
            hasModifier = true;
        } else {
            keyToken = token;
        }
    }

    if (hasModifier) {
        modifiers = parsedModifiers;
    }

    if (!keyToken.empty()) {
        char c = keyToken[0];
        if (c >= 'A' && c <= 'Z') {
            vk = c;
        } else if (c >= '0' && c <= '9') {
            vk = c;
        }
    }
}

int main() {
    HWND hwnd = GetConsoleWindow();
    if (hwnd) {
        ShowWindow(hwnd, SW_HIDE);
    }

    int modifiers = 0;
    int vk = 0;
    parseShortcut(modifiers, vk);

    if (!RegisterHotKey(NULL, 1, modifiers, vk)) {
        return 1;
    }

    std::string chromePath = findChromePath();

    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0) != 0) {
        if (msg.message == WM_HOTKEY) {
            if (msg.wParam == 1) {
                ShellExecuteA(NULL, "open", chromePath.c_str(), NULL, NULL, SW_SHOWNORMAL);
            }
        }
    }
    return 0;
}
