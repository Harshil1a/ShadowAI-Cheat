#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPoint>
#include <QMouseEvent>
#include <QPaintEvent>

class Dashboard;

class MainWindow : public QWidget {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

    Dashboard* dashboard() const { return m_dashboard; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setupUI();
    void applyStyle();

    // Sub-modules
    Dashboard*    m_dashboard    = nullptr;

    // UI elements
    QLabel*         m_titleLabel    = nullptr;
    QPushButton*    m_tbCreditsBtn  = nullptr;
    QPushButton*    m_tbRefreshBtn  = nullptr;
    QPushButton*    m_tbProBtn      = nullptr;
    QPushButton*    m_hideBtn       = nullptr;
    QPushButton*    m_closeBtn      = nullptr;

    void updateTopBarCredits();

    // Window dragging
    bool   m_dragging = false;
    QPoint m_dragOffset;
};

