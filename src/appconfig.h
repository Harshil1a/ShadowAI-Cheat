#pragma once
#include <QString>
#include <QSettings>
#include <QKeySequence>

struct HotkeyConfig {
    int toggleOverlay      = 0;   // Shift+Alt+H
    int takeScreenshot     = 0;   // Shift+Alt+S
    int getAnswer          = 0;   // Shift+Alt+A
    int scrollUp           = 0;   // Shift+Alt+I
    int scrollDown         = 0;   // Shift+Alt+K
    int toggleTransparency = 0;   // Shift+Alt+T
    int clearAnswer        = 0;   // Shift+Alt+Z
    int moveLeft           = 0;   // Shift+Alt+Left
    int moveRight          = 0;   // Shift+Alt+Right
    int moveUp             = 0;   // Shift+Alt+Up
    int moveDown           = 0;   // Shift+Alt+Down
};

class AppConfig {
public:
    static AppConfig& instance();

    // API Settings
    QStringList apiKeys() const;
    QStringList apiProviders() const;
    QStringList apiModels() const;
    QStringList apiBaseUrls() const;
    
    void setApiKeys(const QStringList& keys);
    void setApiProviders(const QStringList& providers);
    void setApiModels(const QStringList& models);
    void setApiBaseUrls(const QStringList& urls);

    int activeSlot() const;
    void setActiveSlot(int slot);

    QString currentApiKey() const;
    QString currentApiProvider() const;
    QString currentApiModel() const;
    QString currentApiBaseUrl() const;

    QString systemPrompt() const;
    void setSystemPrompt(const QString& prompt);

    int maxTokens() const;
    void setMaxTokens(int t);

    int screenshotResolution() const;
    void setScreenshotResolution(int res);

    // Hotkey Settings
    int hotkeyToggle() const;
    void setHotkeyToggle(int vk);

    int hotkeyScreenshot() const;
    void setHotkeyScreenshot(int vk);

    int hotkeyGetAnswer() const;
    void setHotkeyGetAnswer(int vk);

    int hotkeyScrollUp() const;
    void setHotkeyScrollUp(int vk);

    int hotkeyScrollDown() const;
    void setHotkeyScrollDown(int vk);

    int hotkeyTransparency() const;
    void setHotkeyTransparency(int vk);

    int hotkeyClear() const;
    void setHotkeyClear(int vk);

    int hotkeyMoveLeft() const;
    void setHotkeyMoveLeft(int vk);

    int hotkeyMoveRight() const;
    void setHotkeyMoveRight(int vk);

    int hotkeyMoveUp() const;
    void setHotkeyMoveUp(int vk);

    int hotkeyMoveDown() const;
    void setHotkeyMoveDown(int vk);

    int hotkeyVoice() const;
    void setHotkeyVoice(int vk);

    int hotkeyToggleBadges() const;
    void setHotkeyToggleBadges(int vk);

    int hotkeyCopyScreenshot() const;
    void setHotkeyCopyScreenshot(int vk);

    int hotkeyGhostWriter() const;
    void setHotkeyGhostWriter(int vk);

    int hotkeyPanic() const;
    void setHotkeyPanic(int vk);

    int hotkeyHideStrip() const;
    void setHotkeyHideStrip(int vk);

    // Daily Free Queries (3 per day)
    int freeQueriesCountToday();
    int freeQueriesRemaining();
    bool canUseFreeQuery();
    int recordFreeQuery();

    // Rewarded Sponsor Task Free Credits (LootLabs)
    int freeCredits() const;
    void setFreeCredits(int credits);

    int ghostWriterMinDelay() const;
    void setGhostWriterMinDelay(int ms);

    int ghostWriterMaxDelay() const;
    void setGhostWriterMaxDelay(int ms);

    bool ghostWriterSmartIndent() const;
    void setGhostWriterSmartIndent(bool enable);

    // UI Settings
    double overlayOpacity() const;
    void setOverlayOpacity(double opacity);

    int overlayX() const;
    int overlayY() const;
    int overlayWidth() const;
    int overlayHeight() const;
    void setOverlayPos(int x, int y);
    void setOverlaySize(int w, int h);

    // Account & License Settings
    bool isPro() const;
    void setPro(bool pro);

    int proDaysLeft() const;
    void setProDaysLeft(int days);

    QString proPlanTier() const;
    void setProPlanTier(const QString& tier);

    QString userEmail() const;
    void setUserEmail(const QString& email);

    QString licenseKey() const;
    void setLicenseKey(const QString& key);

    bool useProCloudEngine() const;
    void setUseProCloudEngine(bool enable);

    QString proCloudKey() const;
    void setProCloudKey(const QString& key);

    void save();
    void load();

private:
    AppConfig();
    QSettings m_settings;

    bool        m_isPro = false;
    int         m_proDaysLeft = 30;
    QString     m_proPlanTier = "PRO_MONTHLY";
    bool        m_useProCloudEngine = true;
    QString     m_userEmail;
    QString     m_licenseKey;
    QString     m_proCloudKey;
    int         m_freeCredits = 0;

    QStringList m_apiKeys;
    QStringList m_apiProviders;
    QStringList m_apiModels;
    QStringList m_apiBaseUrls;
    int         m_activeSlot;

    QString m_systemPrompt;
    int m_maxTokens;
    int m_screenshotResolution;

    int m_hotkeyToggle;
    int m_hotkeyScreenshot;
    int m_hotkeyGetAnswer;
    int m_hotkeyScrollUp;
    int m_hotkeyScrollDown;
    int m_hotkeyTransparency;
    int m_hotkeyClear;
    int m_hotkeyMoveLeft;
    int m_hotkeyMoveRight;
    int m_hotkeyMoveUp;
    int m_hotkeyMoveDown;
    int m_hotkeyVoice;
    int m_hotkeyToggleBadges;
    int m_hotkeyCopyScreenshot;
    int m_hotkeyGhostWriter;
    int m_hotkeyPanic;
    int m_hotkeyHideStrip;

    int m_ghostWriterMinDelay;
    int m_ghostWriterMaxDelay;
    bool m_ghostWriterSmartIndent;

    double m_overlayOpacity;
    int m_overlayX;
    int m_overlayY;
    int m_overlayWidth;
    int m_overlayHeight;
};
