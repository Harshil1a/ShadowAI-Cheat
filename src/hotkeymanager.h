#pragma once
#include <QObject>

class HotkeyManager : public QObject {
    Q_OBJECT
public:
    explicit HotkeyManager(QObject* parent = nullptr);
    ~HotkeyManager();

    void registerAll();
    void unregisterAll();
    void reregisterAll();  // call after settings change

    // Public method called by global hook to handle intercepted keys
    bool handleHookKey(int vk, bool isDown);

signals:
    void toggleOverlay();
    void takeScreenshot();
    void getAnswer();
    void scrollUp();
    void scrollDown();
    void cycleTransparency();
    void clearAnswer();
    void moveLeft();
    void moveRight();
    void moveUp();
    void moveDown();
    void voiceRecordDown();
    void voiceRecordUp();
    void toggleBadges();
    void copyScreenshot();
    void ghostWriter();
};

