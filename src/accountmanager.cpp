#include "accountmanager.h"
#include "appconfig.h"
#include <QUrl>
#include <QUrlQuery>
#include <QDesktopServices>
#include <QDebug>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSysInfo>
#include <QDateTime>

AccountManager& AccountManager::instance() {
    static AccountManager inst;
    return inst;
}

AccountManager::AccountManager(QObject* parent) : QObject(parent) {
    m_nam = new QNetworkAccessManager(this);
    m_tcpServer = new QTcpServer(this);
    m_creditsPollTimer = new QTimer(this);
    connect(m_creditsPollTimer, &QTimer::timeout, this, &AccountManager::onPollCreditsTimer);

    // Auto-sync Google Account, Pro license, and Free Credits on startup
    QTimer::singleShot(500, this, [this]() {
        syncAccountStatus();
        fetchCloudConfig();
        fetchFreeCredits();
    });
}

AccountManager::~AccountManager() {
    if (m_tcpServer && m_tcpServer->isListening()) {
        m_tcpServer->close();
    }
}

bool AccountManager::isLoggedIn() const {
    return !AppConfig::instance().userEmail().trimmed().isEmpty();
}

bool AccountManager::isPro() const {
    return AppConfig::instance().isPro();
}

QString AccountManager::userEmail() const {
    return AppConfig::instance().userEmail();
}

QString AccountManager::licenseKey() const {
    return AppConfig::instance().licenseKey();
}

QString AccountManager::getMachineHwid() const {
    QByteArray rawHwid = QSysInfo::machineUniqueId();
    QString hwid = QString::fromUtf8(rawHwid.toHex()).toUpper();
    if (hwid.isEmpty()) {
        hwid = QString("%1_%2").arg(QSysInfo::machineHostName(), QSysInfo::currentCpuArchitecture()).toUpper();
    }
    return hwid;
}

void AccountManager::startGoogleLogin() {
    if (!m_tcpServer->isListening()) {
        if (!m_tcpServer->listen(QHostAddress::LocalHost, m_authPort)) {
            emit authError("Could not start local authentication listener on port " + QString::number(m_authPort));
            return;
        }
    }

    QString hwid = getMachineHwid();
    // Launch default browser to real Google OAuth landing bridge on live domain with HWID lock parameter
    QUrl authUrl(QString("https://shadow-ai-cheat.vercel.app/desktop-auth.html?port=%1&hwid=%2").arg(m_authPort).arg(hwid));
    QDesktopServices::openUrl(authUrl);
}

void AccountManager::logout() {
    AppConfig::instance().setUserEmail("");
    AppConfig::instance().setPro(false);
    AppConfig::instance().setLicenseKey("");
    AppConfig::instance().save();

    emit accountStateChanged(false, "", false);
}

void AccountManager::activateLicenseKey(const QString& rawKey) {
    QString key = rawKey.trimmed().toUpper();
    if (key.isEmpty()) {
        emit licenseActivationResult(false, "License key cannot be empty.");
        return;
    }

    // Developer / Master Admin Key Bypass
    if (key == "SHADOW-PRO-HARSHIL-ADMIN") {
        AppConfig::instance().setPro(true);
        AppConfig::instance().setLicenseKey(key);
        AppConfig::instance().save();
        emit licenseActivationResult(true, "✓ Master Developer License Activated! Unlimited PRO Active.");
        emit accountStateChanged(isLoggedIn(), userEmail(), true);
        return;
    }

    // Calculate unique machine hardware ID
    QByteArray rawHwid = QSysInfo::machineUniqueId();
    QString hwid = QString::fromUtf8(rawHwid.toHex()).toUpper();
    if (hwid.isEmpty()) {
        hwid = QString("%1_%2").arg(QSysInfo::machineHostName(), QSysInfo::currentCpuArchitecture()).toUpper();
    }

    // Query Supabase for this license key
    QNetworkRequest req(QUrl(m_supabaseUrl + "/rest/v1/licenses?license_key=eq." + key + "&select=*"));
    req.setRawHeader("apikey", m_supabaseKey.toUtf8());
    req.setRawHeader("Authorization", "Bearer " + m_supabaseKey.toUtf8());

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, key, hwid]() {
        bool foundInDb = false;
        bool isActive = false;
        QString boundHwid;

        bool isExpired = false;
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray resp = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(resp);
            if (doc.isArray() && !doc.array().isEmpty()) {
                foundInDb = true;
                QJsonObject row = doc.array().first().toObject();
                isActive = row.value("is_active").toBool(true);
                boundHwid = row.value("bound_hwid").toString().trimmed().toUpper();

                QString planTier = row.value("plan_tier").toString("PRO_MONTHLY").toUpper();
                QString createdAtStr = row.value("created_at").toString();
                QDateTime createdAt = QDateTime::fromString(createdAtStr, Qt::ISODate);
                if (planTier == "PRO_MONTHLY" && createdAt.isValid()) {
                    if (createdAt.addDays(30) < QDateTime::currentDateTimeUtc()) {
                        isExpired = true;
                    }
                }
            }
        }
        reply->deleteLater();

        if (!foundInDb) {
            emit licenseActivationResult(false, "❌ Invalid license key. Please check your purchase receipt or upgrade to Pro.");
            return;
        }

        if (isExpired) {
            emit licenseActivationResult(false, "❌ MONTHLY LICENSE EXPIRED: Your 30-day Pro access has ended. Please renew on website.");
            return;
        }

        if (!isActive) {
            emit licenseActivationResult(false, "❌ This license key has been revoked or deactivated.");
            return;
        }

        // Hardware lock check:
        if (!boundHwid.isEmpty() && boundHwid != hwid) {
            emit licenseActivationResult(false, "❌ DEVICE LIMIT REACHED: This license is already locked to another computer. Each license is valid for 1 device only.");
            return;
        }

        // Lock to THIS hardware ID and bind this license key to the user's Google ID!
        QJsonObject patchBody;
        if (boundHwid.isEmpty()) {
            patchBody["bound_hwid"] = hwid;
        }
        if (!userEmail().isEmpty()) {
            patchBody["customer_email"] = userEmail();
        }
        patchBody["last_used_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

        QNetworkRequest patchReq(QUrl(m_supabaseUrl + "/rest/v1/licenses?license_key=eq." + key));
        patchReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        patchReq.setRawHeader("apikey", m_supabaseKey.toUtf8());
        patchReq.setRawHeader("Authorization", "Bearer " + m_supabaseKey.toUtf8());
        m_nam->sendCustomRequest(patchReq, "PATCH", QJsonDocument(patchBody).toJson());

        AppConfig::instance().setPro(true);
        AppConfig::instance().setLicenseKey(key);
        AppConfig::instance().save();

        emit licenseActivationResult(true, "✓ License Verified & Locked to this device! PRO Mode Active.");
        emit accountStateChanged(isLoggedIn(), userEmail(), true);
    });
}

void AccountManager::onNewTcpConnection() {
    QTcpSocket* socket = m_tcpServer->nextPendingConnection();
    if (!socket) return;

    connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
        QByteArray data = socket->readAll();
        QString request = QString::fromUtf8(data);
        handleHttpAuthCallback(socket, request);
    });
}

void AccountManager::handleHttpAuthCallback(QTcpSocket* socket, const QString& requestData) {
    QString firstLine = requestData.section("\r\n", 0, 0);
    QString path = firstLine.section(' ', 1, 1);

    // Support CORS preflight
    if (firstLine.startsWith("OPTIONS")) {
        QByteArray corsResp = "HTTP/1.1 204 No Content\r\n"
                              "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: GET, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: *\r\n"
                              "Connection: close\r\n\r\n";
        socket->write(corsResp);
        socket->flush();
        socket->disconnectFromHost();
        return;
    }

    if (path.startsWith("/callback")) {
        QUrl url("http://localhost" + path);
        QUrlQuery query(url);
        QString rawEmail = query.queryItemValue("email");
        QString email = QUrl::fromPercentEncoding(rawEmail.toUtf8()).trimmed();
        QString name = QUrl::fromPercentEncoding(query.queryItemValue("name").toUtf8()).trimmed();
        QString licenseKey = QUrl::fromPercentEncoding(query.queryItemValue("license_key").toUtf8()).trimmed();
        bool isPro = (query.queryItemValue("is_pro") == "1");

        // Clean out legacy mock placeholder
        if (email.contains("operator@gmail.com", Qt::CaseInsensitive) || email.contains("%40")) {
            email = "";
        }

        if (email.isEmpty()) {
            QByteArray resp = "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\nEmpty Email";
            socket->write(resp);
            socket->disconnectFromHost();
            return;
        }

        // Master Developer / Owner Whitelist
        if (email.compare("harshilthakur82@gmail.com", Qt::CaseInsensitive) == 0 ||
            email.contains("harshil", Qt::CaseInsensitive)) {
            isPro = true;
            if (licenseKey.isEmpty()) licenseKey = "SHADOW-PRO-HARSHIL-ADMIN";
            AppConfig::instance().setProDaysLeft(-1);
            AppConfig::instance().setProPlanTier("PRO_OWNER_LIFETIME");
        } else if (isPro) {
            int daysLeft = query.hasQueryItem("days_left") ? query.queryItemValue("days_left").toInt() : 30;
            QString planTier = query.hasQueryItem("plan_tier") ? query.queryItemValue("plan_tier") : "PRO_MONTHLY";
            AppConfig::instance().setProDaysLeft(daysLeft);
            AppConfig::instance().setProPlanTier(planTier);
        }

        // Save authenticated Google session
        AppConfig::instance().setUserEmail(email);
        AppConfig::instance().setPro(isPro);
        if (!licenseKey.isEmpty()) {
            AppConfig::instance().setLicenseKey(licenseKey);
        }

        if (query.hasQueryItem("cloud_key")) {
            QString ckey = query.queryItemValue("cloud_key").trimmed();
            if (!ckey.isEmpty()) {
                AppConfig::instance().setProCloudKey(ckey);
            }
        } else if (isPro) {
            fetchCloudConfig();
        }
        AppConfig::instance().save();

        QString html = 
            "<!DOCTYPE html><html><head><meta charset='utf-8'><title>ShadowAI Connected</title>"
            "<style>"
            "body{background:#030508;color:#00ff66;font-family:monospace;display:flex;align-items:center;justify-content:center;height:100vh;margin:0;}"
            ".box{border:1px solid #00ff66;box-shadow:0 0 35px rgba(0,255,102,0.35);padding:40px;border-radius:10px;text-align:center;max-width:440px;background:#060d09;}"
            "h1{margin-top:0;color:#00ff66;letter-spacing:2px;font-size:24px;}"
            ".badge{display:inline-block;padding:4px 12px;border-radius:4px;font-weight:bold;font-size:12px;margin:12px 0;" + QString(isPro ? "background:rgba(0,255,102,0.2);color:#00ff66;border:1px solid #00ff66;" : "background:rgba(0,229,255,0.2);color:#00e5ff;border:1px solid #00e5ff;") + "}"
            "p{color:#e2fced;font-size:14px;line-height:1.6;}"
            "</style></head><body>"
            "<div class='box'>"
            "<h1>✓ GOOGLE SSO VERIFIED</h1>"
            "<div class='badge'>" + (isPro ? QString("PRO UNLIMITED ACTIVE") : QString("COMMUNITY TIER CONNECTED")) + "</div>"
            "<p>Connected as <strong>" + email.toHtmlEscaped() + "</strong></p>"
            "<p style='color:#7ca88e;font-size:12px;margin-top:20px;'>✓ Session synced with ShadowAI Desktop.<br>You can close this tab and return to the application.</p>"
            "</div></body></html>";

        QByteArray resp = "HTTP/1.1 200 OK\r\n"
                          "Content-Type: text/html; charset=utf-8\r\n"
                          "Access-Control-Allow-Origin: *\r\n"
                          "Connection: close\r\n\r\n" + html.toUtf8();
        socket->write(resp);
        socket->flush();
        socket->disconnectFromHost();

        // If not marked Pro from web callback, double-check database
        if (!isPro) {
            syncAccountStatus();
        }

        emit accountStateChanged(true, email, isPro);

        QTimer::singleShot(2000, this, [this]() {
            if (m_tcpServer && m_tcpServer->isListening()) m_tcpServer->close();
        });
        return;
    }

    // Fallback 404
    socket->write("HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n");
    socket->disconnectFromHost();
}

void AccountManager::syncAccountStatus() {
    QString email = AppConfig::instance().userEmail().trimmed();
    // Clean old corrupted dummy strings
    if (email.contains("%40") || email.contains("operator@gmail.com", Qt::CaseInsensitive)) {
        AppConfig::instance().setUserEmail("");
        AppConfig::instance().setPro(false);
        AppConfig::instance().setLicenseKey("");
        AppConfig::instance().save();
        emit accountStateChanged(false, "", false);
        return;
    }

    if (email.isEmpty()) return;

    // Master Developer / Owner: harshilthakur82@gmail.com is always PRO
    if (email.compare("harshilthakur82@gmail.com", Qt::CaseInsensitive) == 0 ||
        email.contains("harshil", Qt::CaseInsensitive)) {
        AppConfig::instance().setPro(true);
        AppConfig::instance().setLicenseKey("SHADOW-PRO-HARSHIL-ADMIN");
        fetchCloudConfig();
        AppConfig::instance().save();
        emit accountStateChanged(true, email, true);
        return;
    }

    // Query Supabase licenses table for any active license matching this user email
    QUrl url(m_supabaseUrl + "/rest/v1/licenses?customer_email=eq." + QUrl::toPercentEncoding(email) + "&is_active=eq.true&select=*");
    QNetworkRequest req(url);
    req.setRawHeader("apikey", m_supabaseKey.toUtf8());
    req.setRawHeader("Authorization", "Bearer " + m_supabaseKey.toUtf8());

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, email]() {
        bool isPro = false;
        QString key;
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isArray() && !doc.array().isEmpty()) {
                QJsonObject row = doc.array().first().toObject();
                QString planTier = row.value("plan_tier").toString("PRO_MONTHLY").toUpper();
                QString createdAtStr = row.value("created_at").toString();
                QDateTime createdAt = QDateTime::fromString(createdAtStr, Qt::ISODate);
                bool isExpired = false;
                int daysLeft = 30;

                if (planTier == "PRO_MONTHLY" && createdAt.isValid()) {
                    daysLeft = qMax(0, (int)QDateTime::currentDateTimeUtc().daysTo(createdAt.addDays(30)));
                    if (daysLeft == 0 && createdAt.addDays(30) < QDateTime::currentDateTimeUtc()) {
                        isExpired = true;
                    }
                } else if (planTier.contains("LIFETIME") || planTier.contains("GLOBAL_CRYPTO")) {
                    daysLeft = -1; // Perpetual / Lifetime Pro
                }

                if (!isExpired) {
                    QString boundHwid = row.value("bound_hwid").toString().trimmed().toUpper();
                    QString currentHwid = getMachineHwid();

                    if (!boundHwid.isEmpty() && !boundHwid.startsWith("ORDER") && boundHwid != currentHwid) {
                        // Device lock mismatch! Another PC is already using this account
                        isPro = false;
                        AppConfig::instance().setPro(false);
                        AppConfig::instance().save();
                        emit accountStateChanged(true, email, false);
                        emit licenseActivationResult(false, "❌ DEVICE LIMIT: Locked to another computer.");
                        return;
                    }

                    // Bind to this PC on first login
                    if (boundHwid.isEmpty() || boundHwid.startsWith("ORDER")) {
                        QJsonObject patchBody;
                        patchBody["bound_hwid"] = currentHwid;
                        patchBody["last_used_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
                        QNetworkRequest patchReq(QUrl(m_supabaseUrl + "/rest/v1/licenses?customer_email=eq." + QUrl::toPercentEncoding(email)));
                        patchReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
                        patchReq.setRawHeader("apikey", m_supabaseKey.toUtf8());
                        patchReq.setRawHeader("Authorization", "Bearer " + m_supabaseKey.toUtf8());
                        m_nam->sendCustomRequest(patchReq, "PATCH", QJsonDocument(patchBody).toJson());
                    }

                    key = row.value("license_key").toString().trimmed().toUpper();
                    isPro = true;
                    AppConfig::instance().setProDaysLeft(daysLeft);
                    AppConfig::instance().setProPlanTier(planTier);
                }
            }
        }
        reply->deleteLater();

        if (isPro) {
            AppConfig::instance().setPro(true);
            if (!key.isEmpty()) {
                AppConfig::instance().setLicenseKey(key);
            }
            fetchCloudConfig();
            AppConfig::instance().save();
            emit accountStateChanged(true, email, true);
        }
    });
}

void AccountManager::fetchCloudConfig() {
    QUrl url(m_supabaseUrl + "/rest/v1/licenses?customer_email=eq.system@shadowai.local&select=*");
    QNetworkRequest req(url);
    req.setRawHeader("apikey", m_supabaseKey.toUtf8());
    req.setRawHeader("Authorization", "Bearer " + m_supabaseKey.toUtf8());

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isArray() && !doc.array().isEmpty()) {
                QJsonObject row = doc.array().first().toObject();
                QString key = row.value("bound_hwid").toString().trimmed();
                if (!key.isEmpty()) {
                    AppConfig::instance().setProCloudKey(key);
                    AppConfig::instance().save();
                }
            }
        }
        reply->deleteLater();
    });
}

int AccountManager::getFreeCredits() const {
    return AppConfig::instance().freeCredits();
}

void AccountManager::fetchFreeCredits() {
    QString hwid = getMachineHwid();
    QUrl url(QString("https://shadow-ai-cheat.vercel.app/api/user-credits?hwid=%1").arg(QString::fromUtf8(QUrl::toPercentEncoding(hwid))));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "ShadowAI-Desktop-Client");

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                if (obj.value("success").toBool()) {
                    int credits = obj.value("credits").toInt();
                    int previous = AppConfig::instance().freeCredits();
                    AppConfig::instance().setFreeCredits(credits);
                    if (credits != previous) {
                        qDebug() << "[Credits] Synced balance from cloud:" << credits;
                        emit creditsUpdated(credits);
                    }
                }
            }
        }
        reply->deleteLater();
    });
}

void AccountManager::consumeCredit(std::function<void(bool success, int remaining)> callback) {
    if (isPro()) {
        if (callback) callback(true, 999999);
        return;
    }

    // Local optimistic deduction
    int current = AppConfig::instance().freeCredits();
    if (current > 0) {
        AppConfig::instance().setFreeCredits(current - 1);
        emit creditsUpdated(current - 1);
    }

    // Remote sync
    QString hwid = getMachineHwid();
    QUrl url(QString("https://shadow-ai-cheat.vercel.app/api/user-credits?hwid=%1&action=consume").arg(QString::fromUtf8(QUrl::toPercentEncoding(hwid))));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "ShadowAI-Desktop-Client");

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, callback]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                if (obj.value("success").toBool()) {
                    int remaining = obj.value("remaining_credits").toInt();
                    AppConfig::instance().setFreeCredits(remaining);
                    emit creditsUpdated(remaining);
                    if (callback) callback(true, remaining);
                    reply->deleteLater();
                    return;
                }
            }
        }
        if (callback) callback(false, AppConfig::instance().freeCredits());
        reply->deleteLater();
    });
}

void AccountManager::openWatchAdUrl() {
    QString hwid = getMachineHwid();
    QString adLink = QString("https://loot-link.com/s?bz4nCWsI&puid=%1").arg(hwid);
    QDesktopServices::openUrl(QUrl(adLink));

    // Poll for up to 2 minutes (24 checks * 5s) to auto-detect when task completes
    m_pollAttemptsLeft = 24;
    if (m_creditsPollTimer) {
        m_creditsPollTimer->start(5000);
    }
}

void AccountManager::onPollCreditsTimer() {
    m_pollAttemptsLeft--;
    int prevCredits = AppConfig::instance().freeCredits();

    fetchFreeCredits();

    // If new credits were detected or attempts exhausted, stop polling
    if (AppConfig::instance().freeCredits() > prevCredits || m_pollAttemptsLeft <= 0) {
        if (m_creditsPollTimer) {
            m_creditsPollTimer->stop();
        }
    }
}
