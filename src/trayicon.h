#pragma once
#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>

class TrayIcon : public QObject {
    Q_OBJECT
public:
    explicit TrayIcon(QObject* parent = nullptr);
    void show();

signals:
    void toggleOverlay();
    void openSettings();
    void showDashboard();
    void quit();

private:
    QSystemTrayIcon* m_tray;
    QMenu*           m_menu;
    QAction*         m_dashboardAction;
    QAction*         m_toggleAction;
    QAction*         m_settingsAction;
    QAction*         m_quitAction;
};
