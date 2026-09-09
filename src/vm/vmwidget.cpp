#include "vmwidget.h"
#include "vmsetupwizard.h"
#include "vmconsole.h"
#include "vmmanager.h"
#include <QFile>

VMModeWidget::VMModeWidget(QWidget* parent) : QWidget(parent) {
    setupUI();
    applyStyle();
    checkSetupState();
}

VMModeWidget::~VMModeWidget() {}

void VMModeWidget::setupUI() {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_stackedWidget = new QStackedWidget(this);
    layout->addWidget(m_stackedWidget);

    m_setupWizard = new VMSetupWizard(this);
    m_stackedWidget->addWidget(m_setupWizard);

    m_consoleWidget = new VMConsoleWidget(this);
    m_stackedWidget->addWidget(m_consoleWidget);

    // Transitions
    connect(m_setupWizard,   &VMSetupWizard::setupCompleted, this, &VMModeWidget::onSetupCompleted);
    connect(m_consoleWidget, &VMConsoleWidget::setupRequested, this, [this]() {
        m_setupWizard->setForceInstall(true);
        m_stackedWidget->setCurrentIndex(0); // Go back to setup wizard
    });
}

void VMModeWidget::checkSetupState() {
    // Check if RDP is fully configured; if not, show the setup wizard
    if (VMManager::instance().isRdpReady()) {
        m_stackedWidget->setCurrentIndex(1); // Show console
    } else {
        m_stackedWidget->setCurrentIndex(0); // Show setup wizard
    }
}

void VMModeWidget::onSetupCompleted() {
    m_stackedWidget->setCurrentIndex(1);
}

void VMModeWidget::showSettings() {
    // Force settings opening from console
    if (m_stackedWidget->currentIndex() == 1) {
        // Can call console settings directly
    }
}

void VMModeWidget::applyStyle() {
    setStyleSheet(R"(
        VMModeWidget {
            background: transparent;
        }
    )");
}
