#pragma once
#include <QObject>
#include <QString>
#include <QTcpServer>
#include <QTcpSocket>
#include <QNetworkAccessManager>

#include <functional>
#include <QTimer>

class AccountManager : public QObject {
    Q_OBJECT
public:
    static AccountManager& instance();

    bool isLoggedIn() const;
    bool isPro() const;
    QString userEmail() const;
    QString licenseKey() const;

    // Actions
    void startGoogleLogin();
    void logout();
    void activateLicenseKey(const QString& key);
    void syncAccountStatus();
    void fetchCloudConfig();
    QString getMachineHwid() const;

    // Rewarded Free Solve Credits (LootLabs)
    int getFreeCredits() const;
    void fetchFreeCredits();
    void consumeCredit(std::function<void(bool success, int remaining)> callback = nullptr);
    void openWatchAdUrl();

signals:
    void accountStateChanged(bool isLoggedIn, const QString& email, bool isPro);
    void licenseActivationResult(bool success, const QString& message);
    void authError(const QString& message);
    void creditsUpdated(int remainingCredits);

private slots:
    void onNewTcpConnection();
    void onPollCreditsTimer();

private:
    explicit AccountManager(QObject* parent = nullptr);
    ~AccountManager();

    QTcpServer* m_tcpServer = nullptr;
    QNetworkAccessManager* m_nam = nullptr;
    quint16 m_authPort = 18234;

    QString m_supabaseUrl = "https://kptqmelofgromeavgmip.supabase.co";
    QString m_supabaseKey = "sb_publishable_6l5uraxvTrrsbV9PKVJOPg_iKJCqf_J";

    QTimer* m_creditsPollTimer = nullptr;
    int     m_pollAttemptsLeft = 0;

    void handleHttpAuthCallback(QTcpSocket* socket, const QString& requestData);
};
