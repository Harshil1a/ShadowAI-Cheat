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

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void setupUI();
    void applyStyle();

    // Sub-modules
    Dashboard*    m_dashboard    = nullptr;

    // UI elements
    QLabel*         m_titleLabel    = nullptr;
    QPushButton*    m_hideBtn       = nullptr;
    QPushButton*    m_closeBtn      = nullptr;

    // Window dragging
    bool   m_dragging = false;
    QPoint m_dragOffset;
};
