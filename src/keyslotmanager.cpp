#include "keyslotmanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>

KeySlotManager::KeySlotManager(const QStringList& currentKeys, QWidget* parent) 
    : QDialog(parent) 
{
    m_keys = currentKeys;
    // Ensure we have exactly 10 slots
    while (m_keys.size() < 10) m_keys << "";
    if (m_keys.size() > 10) m_keys = m_keys.mid(0, 10);

    setWindowTitle("Blackdor Core — Keys Configuration");
    setFixedSize(500, 500);
    setupUI();
    applyStyle();
}

void KeySlotManager::setupUI() {
    QVBoxLayout* root = new QVBoxLayout(this);
    
    QLabel* header = new QLabel("Assign your API keys to the 10 slots below:");
    header->setStyleSheet("color: #f59e0b; font-weight: bold; margin-bottom: 10px;");
    root->addWidget(header);

    QGridLayout* grid = new QGridLayout;
    grid->setSpacing(10);

    for (int i = 0; i < 10; ++i) {
        QLabel* numLabel = new QLabel(QString("Slot %1:").arg(i + 1));
        numLabel->setFixedWidth(60);

        m_slotLabels[i] = new QLabel(maskKey(m_keys[i]));
        m_slotLabels[i]->setObjectName("slotLabel");
        m_slotLabels[i]->setStyleSheet("background: #000000; border: 2px solid rgba(245,158,11,0.25); border-radius: 0px; padding: 4px; color: #f5f5f4;");

        QPushButton* editBtn = new QPushButton("Edit");
        editBtn->setFixedWidth(60);
        editBtn->setProperty("slotIndex", i);

        QPushButton* clearBtn = new QPushButton("✕");
        clearBtn->setFixedWidth(30);
        clearBtn->setProperty("slotIndex", i);
        clearBtn->setStyleSheet("color: #ef4444;");

        grid->addWidget(numLabel, i, 0);
        grid->addWidget(m_slotLabels[i], i, 1);
        grid->addWidget(editBtn, i, 2);
        grid->addWidget(clearBtn, i, 3);

        connect(editBtn, &QPushButton::clicked, this, &KeySlotManager::onEditSlot);
        connect(clearBtn, &QPushButton::clicked, this, &KeySlotManager::onClearSlot);
    }

    root->addLayout(grid);
    root->addStretch();

    QHBoxLayout* bottom = new QHBoxLayout;
    QPushButton* okBtn = new QPushButton("Apply Changes");
    okBtn->setObjectName("okBtn");
    bottom->addStretch();
    bottom->addWidget(okBtn);
    root->addLayout(bottom);

    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
}

void KeySlotManager::onEditSlot() {
    int index = sender()->property("slotIndex").toInt();
    
    auto btn = QMessageBox::question(this, "Edit API Slot",
        QString("Are you sure you want to modify Slot #%1?").arg(index + 1),
        QMessageBox::Yes | QMessageBox::No);

    if (btn == QMessageBox::Yes) {
        bool ok;
        QString newKey = QInputDialog::getText(this, QString("Slot %1").arg(index+1),
            "Paste new API key:", QLineEdit::Password,
            m_keys[index], &ok);
            
        if (ok) {
            m_keys[index] = newKey.trimmed();
            m_slotLabels[index]->setText(maskKey(m_keys[index]));
        }
    }
}

void KeySlotManager::onClearSlot() {
    int index = sender()->property("slotIndex").toInt();
    m_keys[index] = "";
    m_slotLabels[index]->setText(maskKey(""));
}

QString KeySlotManager::maskKey(const QString& key) {
    if (key.isEmpty()) return "(Empty)";
    if (key.length() <= 10) return "********";
    return key.left(7) + "..." + key.right(4);
}

QStringList KeySlotManager::getKeys() const {
    QStringList result;
    for (const QString& k : m_keys) {
        if (!k.isEmpty()) result << k;
    }
    return result;
}

void KeySlotManager::applyStyle() {
    setStyleSheet(R"(
        QDialog { 
            background: #0c0a09; 
            color: #f5f5f4; 
            border: 2px solid #f59e0b;
        }
        QLabel { font-family: 'Segoe UI', system-ui, sans-serif; font-size: 12px; }
        QPushButton { 
            background: rgba(245, 158, 11, 0.05); 
            border: 2px solid rgba(245, 158, 11, 0.3); 
            border-radius: 0px; /* Sharp */
            color: #f59e0b; 
            font-family: 'Segoe UI', sans-serif;
            padding: 4px;
        }
        QPushButton:hover { background: rgba(245, 158, 11, 0.15); }
        QPushButton#okBtn {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #d97706, stop:1 #b45309);
            border: 2px solid rgba(245, 158, 11, 0.4);
            color: #ffffff;
            font-weight: bold;
            padding: 8px 20px;
            border-radius: 0px; /* Sharp */
        }
        QPushButton#okBtn:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #f59e0b, stop:1 #d97706);
        }
    )");
}