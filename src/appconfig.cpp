#include "appconfig.h"
#include <QStandardPaths>
#include <QDate>

// Default Virtual Key codes (Windows)
// Shift+Alt+H = registered via RegisterHotKey with MOD_SHIFT|MOD_ALT and 'H'
#define DEFAULT_VK_TOGGLE      0x48  // H
#define DEFAULT_VK_SCREENSHOT  0x53  // S
#define DEFAULT_VK_GETANSWER   0x41  // A
#define DEFAULT_VK_MOVE        0x4D  // M
#define DEFAULT_VK_SCROLLUP    0x49  // I
#define DEFAULT_VK_SCROLLDOWN  0x4B  // K
#define DEFAULT_VK_TRANSP      0x54  // T
#define DEFAULT_VK_CLEAR       0x5A  // Z
#define DEFAULT_VK_MOVELEFT    0x25  // Left Arrow
#define DEFAULT_VK_MOVERIGHT   0x27  // Right Arrow
#define DEFAULT_VK_MOVEUP      0x26  // Up Arrow
#define DEFAULT_VK_MOVEDOWN    0x28  // Down Arrow
#define DEFAULT_VK_VOICE       0x52  // R
#define DEFAULT_VK_TOGGLEBADGES 0x42  // B
#define DEFAULT_VK_COPYSCREENSHOT 0x43  // C
#define DEFAULT_VK_GHOSTWRITER 0x56  // V
#define DEFAULT_VK_PANIC       0x2E  // Delete (VK_DELETE)

AppConfig& AppConfig::instance() {
    static AppConfig inst;
    return inst;
}

AppConfig::AppConfig()
    : m_settings("Microsoft", "RuntimeBroker")
{
    load();
}

void AppConfig::load() {
    m_apiKeys      = m_settings.value("api/keys", QStringList()).toStringList();
    m_apiProviders  = m_settings.value("api/providers", QStringList()).toStringList();
    m_apiModels     = m_settings.value("api/models", QStringList()).toStringList();
    m_apiBaseUrls   = m_settings.value("api/baseUrls", QStringList()).toStringList();
    m_activeSlot    = m_settings.value("api/activeSlot", 0).toInt();

    // Ensure all lists are size 10
    while (m_apiKeys.size() < 10) m_apiKeys << "";
    while (m_apiProviders.size() < 10) m_apiProviders << "gemini";
    while (m_apiModels.size() < 10) m_apiModels << "gemini-3.1-flash";
    while (m_apiBaseUrls.size() < 10) m_apiBaseUrls << "";

    m_systemPrompt  = m_settings.value("api/systemPrompt",
        "Analyze the screenshot and provide a helpful, accurate response. "
        "Be concise and follow whatever format best suits the content.").toString();
    
    m_maxTokens = m_settings.value("api/maxTokens", 4096).toInt();
    m_screenshotResolution = m_settings.value("api/screenshotResolution", 720).toInt();

    m_hotkeyToggle      = m_settings.value("hotkeys/toggle",      DEFAULT_VK_TOGGLE).toInt();
    m_hotkeyScreenshot  = m_settings.value("hotkeys/screenshot",  DEFAULT_VK_SCREENSHOT).toInt();
    m_hotkeyGetAnswer   = m_settings.value("hotkeys/getAnswer",   DEFAULT_VK_GETANSWER).toInt();
    m_hotkeyScrollUp    = m_settings.value("hotkeys/scrollUp",    DEFAULT_VK_SCROLLUP).toInt();
    m_hotkeyScrollDown  = m_settings.value("hotkeys/scrollDown",  DEFAULT_VK_SCROLLDOWN).toInt();
    m_hotkeyTransparency= m_settings.value("hotkeys/transparency",DEFAULT_VK_TRANSP).toInt();
    m_hotkeyClear       = m_settings.value("hotkeys/clear",       DEFAULT_VK_CLEAR).toInt();
    m_hotkeyMoveLeft    = m_settings.value("hotkeys/moveLeft",    DEFAULT_VK_MOVELEFT).toInt();
    m_hotkeyMoveRight   = m_settings.value("hotkeys/moveRight",   DEFAULT_VK_MOVERIGHT).toInt();
    m_hotkeyMoveUp      = m_settings.value("hotkeys/moveUp",      DEFAULT_VK_MOVEUP).toInt();
    m_hotkeyMoveDown    = m_settings.value("hotkeys/moveDown",    DEFAULT_VK_MOVEDOWN).toInt();
    m_hotkeyVoice       = m_settings.value("hotkeys/voice",        DEFAULT_VK_VOICE).toInt();
    m_hotkeyToggleBadges = m_settings.value("hotkeys/toggleBadges", DEFAULT_VK_TOGGLEBADGES).toInt();
    m_hotkeyCopyScreenshot = m_settings.value("hotkeys/copyScreenshot", DEFAULT_VK_COPYSCREENSHOT).toInt();
    m_hotkeyGhostWriter = m_settings.value("hotkeys/ghostWriter", DEFAULT_VK_GHOSTWRITER).toInt();
    m_hotkeyPanic       = m_settings.value("hotkeys/panic",       DEFAULT_VK_PANIC).toInt();

    m_ghostWriterMinDelay = m_settings.value("ghostwriter/minDelay", 15).toInt();
    m_ghostWriterMaxDelay = m_settings.value("ghostwriter/maxDelay", 30).toInt();
    m_ghostWriterSmartIndent = m_settings.value("ghostwriter/smartIndent", true).toBool();

    m_overlayOpacity = m_settings.value("ui/opacity", 0.85).toDouble();
    m_overlayX       = m_settings.value("ui/x", 100).toInt();
    m_overlayY       = m_settings.value("ui/y", 100).toInt();
    m_overlayWidth   = m_settings.value("ui/width", 720).toInt();
    m_overlayHeight  = m_settings.value("ui/height", 680).toInt();

    m_isPro          = m_settings.value("account/isPro", false).toBool();
    m_useProCloudEngine = m_settings.value("api/useProCloudEngine", true).toBool();
    m_userEmail      = m_settings.value("account/email", "").toString();
    m_licenseKey     = m_settings.value("account/licenseKey", "").toString();
}

void AppConfig::save() {
    m_settings.setValue("account/isPro",      m_isPro);
    m_settings.setValue("api/useProCloudEngine", m_useProCloudEngine);
    m_settings.setValue("account/email",      m_userEmail);
    m_settings.setValue("account/licenseKey", m_licenseKey);

    m_settings.setValue("api/keys",         m_apiKeys);
    m_settings.setValue("api/providers",    m_apiProviders);
    m_settings.setValue("api/models",       m_apiModels);
    m_settings.setValue("api/baseUrls",     m_apiBaseUrls);
    m_settings.setValue("api/activeSlot",   m_activeSlot);
    m_settings.setValue("api/systemPrompt", m_systemPrompt);
    m_settings.setValue("api/maxTokens",    m_maxTokens);
    m_settings.setValue("api/screenshotResolution", m_screenshotResolution);

    m_settings.setValue("hotkeys/toggle",       m_hotkeyToggle);
    m_settings.setValue("hotkeys/screenshot",   m_hotkeyScreenshot);
    m_settings.setValue("hotkeys/getAnswer",    m_hotkeyGetAnswer);
    m_settings.setValue("hotkeys/scrollUp",     m_hotkeyScrollUp);
    m_settings.setValue("hotkeys/scrollDown",   m_hotkeyScrollDown);
    m_settings.setValue("hotkeys/transparency", m_hotkeyTransparency);
    m_settings.setValue("hotkeys/clear",        m_hotkeyClear);
    m_settings.setValue("hotkeys/moveLeft",     m_hotkeyMoveLeft);
    m_settings.setValue("hotkeys/moveRight",    m_hotkeyMoveRight);
    m_settings.setValue("hotkeys/moveUp",       m_hotkeyMoveUp);
    m_settings.setValue("hotkeys/moveDown",     m_hotkeyMoveDown);
    m_settings.setValue("hotkeys/voice",        m_hotkeyVoice);
    m_settings.setValue("hotkeys/toggleBadges", m_hotkeyToggleBadges);
    m_settings.setValue("hotkeys/copyScreenshot", m_hotkeyCopyScreenshot);
    m_settings.setValue("hotkeys/ghostWriter", m_hotkeyGhostWriter);
    m_settings.setValue("hotkeys/panic",       m_hotkeyPanic);
    m_settings.setValue("ghostwriter/minDelay", m_ghostWriterMinDelay);
    m_settings.setValue("ghostwriter/maxDelay", m_ghostWriterMaxDelay);
    m_settings.setValue("ghostwriter/smartIndent", m_ghostWriterSmartIndent);

    m_settings.setValue("ui/opacity", m_overlayOpacity);
    m_settings.setValue("ui/x",       m_overlayX);
    m_settings.setValue("ui/y",       m_overlayY);
    m_settings.setValue("ui/width",   m_overlayWidth);
    m_settings.setValue("ui/height",  m_overlayHeight);

    m_settings.sync();
}

// --- Getters / Setters ---
QStringList AppConfig::apiKeys() const          { return m_apiKeys; }
void AppConfig::setApiKeys(const QStringList& k){ m_apiKeys = k; }
QStringList AppConfig::apiProviders() const      { return m_apiProviders; }
void AppConfig::setApiProviders(const QStringList& p){ m_apiProviders = p; }
QStringList AppConfig::apiModels() const         { return m_apiModels; }
void AppConfig::setApiModels(const QStringList& m){ m_apiModels = m; }
QStringList AppConfig::apiBaseUrls() const       { return m_apiBaseUrls; }
void AppConfig::setApiBaseUrls(const QStringList& u){ m_apiBaseUrls = u; }

int AppConfig::activeSlot() const { return m_activeSlot; }
void AppConfig::setActiveSlot(int slot) { m_activeSlot = slot; }

QString AppConfig::currentApiKey() const {
    if (m_activeSlot >= 0 && m_activeSlot < m_apiKeys.size()) return m_apiKeys[m_activeSlot];
    return "";
}

QString AppConfig::currentApiProvider() const {
    if (m_activeSlot >= 0 && m_activeSlot < m_apiProviders.size()) return m_apiProviders[m_activeSlot];
    return "gemini";
}

QString AppConfig::currentApiModel() const {
    if (m_activeSlot >= 0 && m_activeSlot < m_apiModels.size()) return m_apiModels[m_activeSlot];
    return "gemini-2.0-flash";
}

QString AppConfig::currentApiBaseUrl() const {
    if (m_activeSlot >= 0 && m_activeSlot < m_apiBaseUrls.size()) return m_apiBaseUrls[m_activeSlot];
    return "";
}
QString AppConfig::systemPrompt() const    { return m_systemPrompt; }
void AppConfig::setSystemPrompt(const QString& s){ m_systemPrompt = s; }
int AppConfig::maxTokens() const           { return m_maxTokens; }
void AppConfig::setMaxTokens(int t)        { m_maxTokens = t; }
int AppConfig::screenshotResolution() const { return m_screenshotResolution; }
void AppConfig::setScreenshotResolution(int res) { m_screenshotResolution = res; }

int AppConfig::hotkeyToggle() const       { return m_hotkeyToggle; }
void AppConfig::setHotkeyToggle(int v)    { m_hotkeyToggle = v; }
int AppConfig::hotkeyScreenshot() const   { return m_hotkeyScreenshot; }
void AppConfig::setHotkeyScreenshot(int v){ m_hotkeyScreenshot = v; }
int AppConfig::hotkeyGetAnswer() const    { return m_hotkeyGetAnswer; }
void AppConfig::setHotkeyGetAnswer(int v) { m_hotkeyGetAnswer = v; }
int AppConfig::hotkeyScrollUp() const     { return m_hotkeyScrollUp; }
void AppConfig::setHotkeyScrollUp(int v)  { m_hotkeyScrollUp = v; }
int AppConfig::hotkeyScrollDown() const   { return m_hotkeyScrollDown; }
void AppConfig::setHotkeyScrollDown(int v){ m_hotkeyScrollDown = v; }
int AppConfig::hotkeyTransparency() const { return m_hotkeyTransparency; }
void AppConfig::setHotkeyTransparency(int v){ m_hotkeyTransparency = v; }
int AppConfig::hotkeyClear() const        { return m_hotkeyClear; }
void AppConfig::setHotkeyClear(int v)     { m_hotkeyClear = v; }
int AppConfig::hotkeyMoveLeft() const     { return m_hotkeyMoveLeft; }
void AppConfig::setHotkeyMoveLeft(int v)  { m_hotkeyMoveLeft = v; }
int AppConfig::hotkeyMoveRight() const    { return m_hotkeyMoveRight; }
void AppConfig::setHotkeyMoveRight(int v) { m_hotkeyMoveRight = v; }
int AppConfig::hotkeyMoveUp() const       { return m_hotkeyMoveUp; }
void AppConfig::setHotkeyMoveUp(int v)    { m_hotkeyMoveUp = v; }
int AppConfig::hotkeyMoveDown() const     { return m_hotkeyMoveDown; }
void AppConfig::setHotkeyMoveDown(int v)  { m_hotkeyMoveDown = v; }
int AppConfig::hotkeyVoice() const        { return m_hotkeyVoice; }
void AppConfig::setHotkeyVoice(int v)     { m_hotkeyVoice = v; }
int AppConfig::hotkeyToggleBadges() const { return m_hotkeyToggleBadges; }
void AppConfig::setHotkeyToggleBadges(int v) { m_hotkeyToggleBadges = v; }

int AppConfig::hotkeyCopyScreenshot() const { return m_hotkeyCopyScreenshot; }
void AppConfig::setHotkeyCopyScreenshot(int v) { m_hotkeyCopyScreenshot = v; }

int AppConfig::hotkeyGhostWriter() const { return m_hotkeyGhostWriter; }
void AppConfig::setHotkeyGhostWriter(int v) { m_hotkeyGhostWriter = v; }

int AppConfig::ghostWriterMinDelay() const { return m_ghostWriterMinDelay; }
void AppConfig::setGhostWriterMinDelay(int ms) { m_ghostWriterMinDelay = ms; }

int AppConfig::ghostWriterMaxDelay() const { return m_ghostWriterMaxDelay; }
void AppConfig::setGhostWriterMaxDelay(int ms) { m_ghostWriterMaxDelay = ms; }

bool AppConfig::ghostWriterSmartIndent() const { return m_ghostWriterSmartIndent; }
void AppConfig::setGhostWriterSmartIndent(bool enable) { m_ghostWriterSmartIndent = enable; }

double AppConfig::overlayOpacity() const  { return m_overlayOpacity; }
void AppConfig::setOverlayOpacity(double o){ m_overlayOpacity = o; }
int AppConfig::overlayX() const           { return m_overlayX; }
int AppConfig::overlayY() const           { return m_overlayY; }
int AppConfig::overlayWidth() const       { return m_overlayWidth; }
int AppConfig::overlayHeight() const      { return m_overlayHeight; }
void AppConfig::setOverlayPos(int x, int y){ m_overlayX = x; m_overlayY = y; }
void AppConfig::setOverlaySize(int w, int h){ m_overlayWidth = w; m_overlayHeight = h; }

bool AppConfig::isPro() const { return m_isPro; }
void AppConfig::setPro(bool pro) { m_isPro = pro; }

QString AppConfig::userEmail() const { return m_userEmail; }
void AppConfig::setUserEmail(const QString& email) { m_userEmail = email; }

QString AppConfig::licenseKey() const { return m_licenseKey; }
void AppConfig::setLicenseKey(const QString& key) { m_licenseKey = key; }

bool AppConfig::useProCloudEngine() const { return m_useProCloudEngine; }
void AppConfig::setUseProCloudEngine(bool enable) { m_useProCloudEngine = enable; }

int AppConfig::hotkeyPanic() const { return m_hotkeyPanic; }
void AppConfig::setHotkeyPanic(int vk) { m_hotkeyPanic = vk; }

int AppConfig::freeQueriesCountToday() {
    QString today = QDate::currentDate().toString("yyyy-MM-dd");
    QString savedDate = m_settings.value("freeTier/date", "").toString();
    if (savedDate != today) {
        return 0;
    }
    return m_settings.value("freeTier/count", 0).toInt();
}

int AppConfig::freeQueriesRemaining() {
    return qMax(0, 3 - freeQueriesCountToday());
}

bool AppConfig::canUseFreeQuery() {
    if (isPro()) return true;
    return freeQueriesCountToday() < 3;
}

int AppConfig::recordFreeQuery() {
    QString today = QDate::currentDate().toString("yyyy-MM-dd");
    QString savedDate = m_settings.value("freeTier/date", "").toString();
    int count = 0;
    if (savedDate == today) {
        count = m_settings.value("freeTier/count", 0).toInt();
    }
    count++;
    m_settings.setValue("freeTier/date", today);
    m_settings.setValue("freeTier/count", count);
    return count;
}



