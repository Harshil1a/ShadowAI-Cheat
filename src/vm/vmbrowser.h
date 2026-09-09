#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTextBrowser>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class VMBrowserOverlay : public QWidget {
    Q_OBJECT
public:
    explicit VMBrowserOverlay(QWidget* parent = nullptr);
    ~VMBrowserOverlay();

    void triggerGlobalShortcut();

signals:
    void captureRequested(int mode); // 0 = Full, 1 = Window, 2 = Region
    void logMessage(const QString& msg);

private slots:
    void onNavigate();
    void onNewTab();
    void onCloseTab(int index);
    void onNetworkReply(QNetworkReply* reply);
    void handleCapture(int mode);

private:
    void setupUI();
    void applyStyle();
    void loadUrl(const QString& url, QTextBrowser* browser);

    QTabWidget*    m_tabs           = nullptr;
    QLineEdit*     m_addressBar     = nullptr;
    QPushButton*   m_backBtn        = nullptr;
    QPushButton*   m_forwardBtn     = nullptr;
    QPushButton*   m_refreshBtn     = nullptr;
    QPushButton*   m_searchBtn      = nullptr;
    QPushButton*   m_newTabBtn      = nullptr;

    // Screenshot actions
    QPushButton*   m_capFullBtn     = nullptr;
    QPushButton*   m_capWinBtn      = nullptr;
    QPushButton*   m_capRegBtn      = nullptr;

    QNetworkAccessManager* m_nam    = nullptr;
    QMap<QNetworkReply*, QTextBrowser*> m_activeRequests;
};
