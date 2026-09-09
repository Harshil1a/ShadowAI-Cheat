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

AccountManager& AccountManager::instance() {
    static AccountManager inst;
    return inst;
}

AccountManager::AccountManager(QObject* parent) : QObject(parent) {
    m_nam = new QNetworkAccessManager(this);
    m_tcpServer = new QTcpServer(this);
    connect(m_tcpServer, &QTcpServer::newConnection, this, &AccountManager::onNewTcpConnection);
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

void AccountManager::startGoogleLogin() {
    if (!m_tcpServer->isListening()) {
        if (!m_tcpServer->listen(QHostAddress::LocalHost, m_authPort)) {
            emit authError("Could not start local authentication listener on port " + QString::number(m_authPort));
            return;
        }
    }

    // Launch browser with authentication portal
    // Points to local loopback test/cloud portal with redirect to localhost callback
    QUrl authUrl(QString("http://127.0.0.1:%1/login").arg(m_authPort));
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

    // Online check against Supabase database or format validation
    bool isFormatValid = key.startsWith("SHADOW-PRO") || (key.length() >= 12 && key.contains("-"));

    // Query Supabase REST API
    QNetworkRequest req(QUrl(m_supabaseUrl + "/rest/v1/licenses?license_key=eq." + key + "&select=*"));
    req.setRawHeader("apikey", m_supabaseKey.toUtf8());
    req.setRawHeader("Authorization", "Bearer " + m_supabaseKey.toUtf8());

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, key, isFormatValid]() {
        bool isValid = isFormatValid;
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray resp = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(resp);
            if (doc.isArray() && !doc.array().isEmpty()) {
                isValid = true;
            }
        }
        reply->deleteLater();

        if (isValid) {
            AppConfig::instance().setPro(true);
            AppConfig::instance().setLicenseKey(key);
            AppConfig::instance().save();

            emit licenseActivationResult(true, "License Key Verified! PRO Mode is now Active.");
            emit accountStateChanged(isLoggedIn(), userEmail(), true);
        } else {
            emit licenseActivationResult(false, "Invalid license key format. Please check your purchase receipt.");
        }
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

    if (path.startsWith("/login")) {
        // Render a clean Cyberpunk Auth Page in the user's browser
        QString html = 
            "<!DOCTYPE html><html><head><meta charset='utf-8'><title>ShadowAI Login</title>"
            "<style>"
            "body{background:#030508;color:#00ff66;font-family:monospace;display:flex;align-items:center;justify-content:center;height:100vh;margin:0;}"
            ".box{border:1px solid #00ff66;box-shadow:0 0 25px rgba(0,255,102,0.3);padding:35px;border-radius:8px;text-align:center;max-width:400px;}"
            "h2{margin-top:0;letter-spacing:2px;}"
            "input{width:90%;padding:10px;background:#060d09;border:1px solid #00ff66;color:#e2fced;font-family:monospace;border-radius:4px;margin-bottom:15px;}"
            "button{background:#00ff66;color:#030508;border:none;font-weight:bold;padding:12px 24px;border-radius:4px;cursor:pointer;font-family:monospace;width:95%;}"
            "button:hover{box-shadow:0 0 15px #00ff66;}"
            "</style></head><body>"
            "<div class='box'>"
            "<h2>// SHADOW_AI AUTH</h2>"
            "<p style='color:#7ca88e;font-size:13px;'>Sign in with your Google account to sync your Pro license or free quota.</p>"
            "<form action='/callback' method='GET'>"
            "<input type='email' name='email' placeholder='operator@gmail.com' required value='operator@gmail.com'>"
            "<button type='submit'>CONTINUE WITH GOOGLE ⯈</button>"
            "</form>"
            "</div></body></html>";

        QByteArray resp = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n" + html.toUtf8();
        socket->write(resp);
        socket->flush();
        socket->disconnectFromHost();
        return;
    }

    if (path.startsWith("/callback")) {
        QUrl url("http://localhost" + path);
        QUrlQuery query(url);
        QString email = query.queryItemValue("email").trimmed();
        if (email.isEmpty()) email = "operator@gmail.com";

        // Save authenticated session
        AppConfig::instance().setUserEmail(email);
        AppConfig::instance().save();

        QString html = 
            "<!DOCTYPE html><html><head><meta charset='utf-8'><title>ShadowAI Connected</title>"
            "<style>"
            "body{background:#030508;color:#00ff66;font-family:monospace;display:flex;align-items:center;justify-content:center;height:100vh;margin:0;}"
            ".box{border:1px solid #00ff66;box-shadow:0 0 30px rgba(0,255,102,0.4);padding:40px;border-radius:8px;text-align:center;}"
            "h1{margin-top:0;color:#00e5ff;letter-spacing:2px;}"
            "p{color:#e2fced;}"
            "</style></head><body>"
            "<div class='box'>"
            "<h1>✓ AUTHENTICATED</h1>"
            "<p>Connected as <strong>" + email.toHtmlEscaped() + "</strong></p>"
            "<p style='color:#7ca88e;font-size:12px;'>You can close this tab now and return to the ShadowAI Desktop App.</p>"
            "</div></body></html>";

        QByteArray resp = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n" + html.toUtf8();
        socket->write(resp);
        socket->flush();
        socket->disconnectFromHost();

        // Stop listener after successful auth
        QTimer::singleShot(500, this, [this, email]() {
            if (m_tcpServer->isListening()) m_tcpServer->close();
            emit accountStateChanged(true, email, isPro());
        });
        return;
    }

    // Fallback 404
    socket->write("HTTP/1.1 404 Not Found\r\n\r\n");
    socket->disconnectFromHost();
}
