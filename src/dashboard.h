#pragma once
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QMouseEvent>
#include <QPainter>
#include <QPoint>
#include "settingswindow.h"

class Dashboard : public QWidget {
    Q_OBJECT
public:
    explicit Dashboard(QWidget* parent = nullptr);

    void openSettingsPage();
    SettingsWindow* settingsWidget() const { return m_settingsWidget; }
    bool isOverlayRunning() const { return m_isOverlayRunning; }

signals:
    void toggleOverlay();
    void hideOverlay();
    void openSettings();
    void quitApp();

public slots:
    void updateStatus(bool active, bool visible = true);
    void checkInternet();

protected:
    void closeEvent(QCloseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void showEvent(QShowEvent* event) override;

private slots:
    void onInternetResult(QNetworkReply* reply);
    void checkForUpdates();
    void onVersionCheckReply(QNetworkReply* reply);

private:
    void setupUI();
    void applyStyle();

    SettingsWindow* m_settingsWidget   = nullptr;
    QLabel*         m_titleLabel       = nullptr;
    QLabel*         m_statusLabel      = nullptr;
    QPushButton*    m_updateBadge      = nullptr;
    QPushButton*    m_toggleBtn        = nullptr;
    QPushButton*    m_settingsBtn      = nullptr;
    QPushButton*    m_exitBtn          = nullptr;
    // Custom title bar buttons
    QPushButton*    m_hideBtn          = nullptr;  // — hides overlay, keeps app running
    QPushButton*    m_closeBtn         = nullptr;  // × quits app

    // Account & License UI
    QLabel*         m_accountBadge     = nullptr;
    QPushButton*    m_loginBtn         = nullptr;
    QPushButton*    m_proBtn           = nullptr;
    QPushButton*    m_creditsBtn       = nullptr;
    void refreshAccountUI();

    QNetworkAccessManager* m_nam       = nullptr;
    bool            m_isOverlayRunning = false;
    bool            m_isOnline         = true;

    // For frameless window dragging
    bool            m_dragging         = false;
    QPoint          m_dragOffset;
};
