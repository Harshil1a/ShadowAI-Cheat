#pragma once
#include <QWidget>
#include <QStackedWidget>
#include <QVBoxLayout>

class VMSetupWizard;
class VMConsoleWidget;

class VMModeWidget : public QWidget {
    Q_OBJECT
public:
    explicit VMModeWidget(QWidget* parent = nullptr);
    ~VMModeWidget();

    void showSettings();

private slots:
    void onSetupCompleted();

private:
    void setupUI();
    void applyStyle();
    void checkSetupState();

    QStackedWidget*   m_stackedWidget = nullptr;
    VMSetupWizard*    m_setupWizard   = nullptr;
    VMConsoleWidget*  m_consoleWidget = nullptr;
};
