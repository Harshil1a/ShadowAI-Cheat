#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QTextEdit>
#include <QVBoxLayout>


class VMSetupWizard : public QWidget {
    Q_OBJECT
public:
    explicit VMSetupWizard(QWidget* parent = nullptr);
    ~VMSetupWizard();
    void setForceInstall(bool force) { m_forceInstall = force; }

signals:
    void setupCompleted();

private slots:
    void onStartSetup();
    void onSimulateClicked();

private:
    void setupUI();
    void applyStyle();
    void addLog(const QString& msg, bool isError = false);
    void addSuccess(const QString& msg);
    void addWarning(const QString& msg);

    // Sequential in-app setup steps
    void runStep1_CheckSystem();
    void runStep2_DefenderExclusion();
    void runStep3_InstallRdpWrapper();
    void runStep4_CreateUser();
    void runStep5_EnableRdpAccess();
    void runStep6_Verify();
    void setupComplete();
    void setupFailed(const QString& reason);

    void setProgress(int step, int total);
    void setButtonState(const QString& text, bool enabled);

    // State tracking
    int  m_currentStep = 0;
    int  m_totalSteps = 6;
    bool m_forceInstall = false;

    // UI elements
    QLabel*       m_titleLabel       = nullptr;
    QLabel*       m_subtitleLabel    = nullptr;
    QLabel*       m_infoLabel        = nullptr;
    QPushButton*  m_startBtn         = nullptr;
    QPushButton*  m_simulateBtn      = nullptr;
    QProgressBar* m_progressBar      = nullptr;
    QTextEdit*    m_logDisplay       = nullptr;
    QLabel*       m_stepLabel        = nullptr;
};
