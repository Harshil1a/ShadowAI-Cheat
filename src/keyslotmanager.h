#pragma once
#include <QDialog>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QStringList>

class KeySlotManager : public QDialog {
    Q_OBJECT
public:
    explicit KeySlotManager(const QStringList& currentKeys, QWidget* parent = nullptr);
    QStringList getKeys() const;

private slots:
    void onEditSlot();
    void onClearSlot();

private:
    void setupUI();
    void applyStyle();
    QString maskKey(const QString& key);

    QStringList m_keys;
    QLabel* m_slotLabels[10];
};
