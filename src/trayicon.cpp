#include "trayicon.h"
#include <QApplication>
#include <QIcon>
#include <QPixmap>
#include <QPainter>

// Generate a simple icon programmatically (cyan diamond on dark bg)
static QIcon generateIcon() {
    QPixmap px(32, 32);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);

    // Dark background circle
    p.setBrush(QColor(4, 18, 34, 200));
    p.setPen(Qt::NoPen);
    p.drawEllipse(1, 1, 30, 30);

    // Cyan diamond shape
    QPolygon diamond;
    diamond << QPoint(16, 4)
            << QPoint(28, 16)
            << QPoint(16, 28)
            << QPoint(4, 16);

    p.setBrush(QColor(0, 212, 255, 180));
    p.setPen(QPen(QColor(0, 212, 255, 220), 1));
    p.drawPolygon(diamond);

    // Inner dot
    p.setBrush(QColor(255, 255, 255, 200));
    p.setPen(Qt::NoPen);
    p.drawEllipse(13, 13, 6, 6);

    return QIcon(px);
}

TrayIcon::TrayIcon(QObject* parent)
    : QObject(parent)
{
    m_tray = new QSystemTrayIcon(generateIcon(), this);
    m_tray->setToolTip("Runtime Broker");

    m_menu = new QMenu();
    m_menu->setStyleSheet(R"(
        QMenu {
            background: #0a1628;
            border: 1px solid rgba(0, 212, 255, 0.3);
            color: #c8e0f4;
            font-family: Consolas;
            font-size: 12px;
            padding: 4px;
            border-radius: 0px;
        }
        QMenu::item {
            padding: 6px 20px;
            border-radius: 0px;
        }
        QMenu::item:selected {
            background: rgba(0, 212, 255, 0.2);
            color: #00d4ff;
        }
        QMenu::separator {
            height: 1px;
            background: rgba(0, 212, 255, 0.15);
            margin: 3px 8px;
        }
    )");

    m_dashboardAction = m_menu->addAction("▣  Show Dashboard");
#if defined(Q_OS_MAC) || defined(Q_OS_MACOS)
    m_toggleAction    = m_menu->addAction("⊡  Toggle Overlay  (Shift+Option+H)");
#else
    m_toggleAction    = m_menu->addAction("⊡  Toggle Overlay  (Shift+Alt+H)");
#endif
    m_menu->addSeparator();
    m_settingsAction  = m_menu->addAction("⚙  Settings");
    m_menu->addSeparator();
    m_quitAction      = m_menu->addAction("✕  Quit");

    m_tray->setContextMenu(m_menu);

    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick || reason == QSystemTrayIcon::Trigger) {
            emit showDashboard();
        }
    });

    connect(m_dashboardAction, &QAction::triggered, this, &TrayIcon::showDashboard);

    connect(m_toggleAction,   &QAction::triggered, this, &TrayIcon::toggleOverlay);
    connect(m_settingsAction, &QAction::triggered, this, &TrayIcon::openSettings);
    connect(m_quitAction,     &QAction::triggered, this, &TrayIcon::quit);
}

void TrayIcon::show() {
    m_tray->show();
}
