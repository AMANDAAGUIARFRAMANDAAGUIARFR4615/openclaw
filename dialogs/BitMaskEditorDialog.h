#pragma once

#include "AppSettingsDialog.h"
#include <QDialog>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QList>
#include <QPushButton>

class BitMaskEditorDialog : public QDialog
{
    Q_OBJECT
public:
    struct Item {
        int bit;
        QString name;
        int scale = 100;
        std::optional<int> isLandscape;
        std::optional<int> videoFps;
        std::optional<int> videoQuality;
        std::optional<int> connectionMethod;
        std::optional<int> autoScanLANDevices;
        std::optional<int> autoConnectUSBDevices;

        int getIsLandscape() { return isLandscape.value_or(AppSettingsDialog::getInstance()->getValue("isLandscape")); }
        int getVideoFps() { return videoFps.value_or(AppSettingsDialog::getInstance()->getValue("videoFps")); }
        int getVideoQuality() { return videoQuality.value_or(AppSettingsDialog::getInstance()->getValue("videoQuality")); }
        int getConnectionMethod() { return connectionMethod.value_or(AppSettingsDialog::getInstance()->getValue("connectionMethod")); }
        int getAutoScanLANDevices() { return autoScanLANDevices.value_or(AppSettingsDialog::getInstance()->getValue("autoScanLANDevices")); }
        int getAutoConnectUSBDevices() { return autoConnectUSBDevices.value_or(AppSettingsDialog::getInstance()->getValue("autoConnectUSBDevices")); }

        void load(int index)
        {
            settings->setArrayIndex(index);

            bit = settings->value("bit").toInt();
            name = settings->value("name").toString();
            scale = settings->value("scale").toInt();

            if (settings->contains("isLandscape"))
                isLandscape = settings->value("isLandscape").toInt();
            else
                isLandscape = std::nullopt;

            if (settings->contains("videoFps"))
                videoFps = settings->value("videoFps").toInt(); 
            else
                videoFps = std::nullopt;

            if (settings->contains("videoQuality"))
                videoQuality = settings->value("videoQuality").toInt(); 
            else
                videoQuality = std::nullopt;

            if (settings->contains("connectionMethod"))
                connectionMethod = settings->value("connectionMethod").toInt();
            else
                connectionMethod = std::nullopt;

            if (settings->contains("autoScanLANDevices"))
                autoScanLANDevices = settings->value("autoScanLANDevices").toInt();
            else
                autoScanLANDevices = std::nullopt;

            if (settings->contains("autoConnectUSBDevices"))
                autoConnectUSBDevices = settings->value("autoConnectUSBDevices").toInt();
            else
                autoConnectUSBDevices = std::nullopt;
        }

        void save(int index)
        {
            settings->setArrayIndex(index);
            settings->setValue("bit", bit);
            settings->setValue("name", name);
            settings->setValue("scale", scale);

            if (isLandscape.has_value())
                settings->setValue("isLandscape", isLandscape.value());
            else
                settings->remove("isLandscape");

            if (videoFps.has_value())
                settings->setValue("videoFps", videoFps.value());
            else
                settings->remove("videoFps");

            if (videoQuality.has_value())
                settings->setValue("videoQuality", videoQuality.value());
            else
                settings->remove("videoQuality");

            if (connectionMethod.has_value())
                settings->setValue("connectionMethod", connectionMethod.value());
            else
                settings->remove("connectionMethod");

            if (autoScanLANDevices.has_value())
                settings->setValue("autoScanLANDevices", autoScanLANDevices.value());
            else
                settings->remove("autoScanLANDevices");

            if (autoConnectUSBDevices.has_value())
                settings->setValue("autoConnectUSBDevices", autoConnectUSBDevices.value());
            else
                settings->remove("autoConnectUSBDevices");
        }
    };

    explicit BitMaskEditorDialog(const QList<Item>& items,
                                 const QList<quint32*>& maskRefs,
                                 QWidget* parent = nullptr)
        : QDialog(parent)
        , m_items(items)
        , m_maskRefs(maskRefs)
    {
        setWindowTitle("位掩码编辑器");
        setupUi();
    }

private:
    void setupUi()
    {
        auto mainLayout = new QVBoxLayout(this);

        auto grid = new QGridLayout;
        grid->setHorizontalSpacing(20);
        grid->setVerticalSpacing(10);
        m_checkBoxes.reserve(m_items.size());

        for (int i = 0; i < m_items.size(); ++i) {
            const auto& item = m_items[i];

            auto cb = new QCheckBox(item.name, this);
            
            // 统计当前分组在多少个设备中是勾选状态
            int checkedCount = 0;
            for (quint32* mask : m_maskRefs) {
                if (*mask & (1U << item.bit)) {
                    checkedCount++;
                }
            }

            // 根据包含该位掩码的设备数量决定 CheckBox 的状态
            if (m_maskRefs.isEmpty() || checkedCount == 0) {
                cb->setCheckState(Qt::Unchecked);
            } else if (checkedCount == m_maskRefs.size()) {
                cb->setCheckState(Qt::Checked);
            } else {
                cb->setTristate(true);
                cb->setCheckState(Qt::PartiallyChecked);
            }
            
            if (!item.bit) {
                cb->setCheckState(Qt::Checked);
                cb->setEnabled(false);
            }

            m_checkBoxes.push_back(cb);

            int col = static_cast<int>(i % 4);
            int row = static_cast<int>(i / 4);
            grid->addWidget(cb, row, col);
        }

        mainLayout->addLayout(grid);
        mainLayout->addStretch();

        auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        buttonBox->button(QDialogButtonBox::Ok)->setText("确定");
        buttonBox->button(QDialogButtonBox::Cancel)->setText("取消");

        mainLayout->addWidget(buttonBox);

        connect(buttonBox, &QDialogButtonBox::accepted, this, &BitMaskEditorDialog::collectMask);
        connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }

    void collectMask()
    {
        for (int i = 0; i < m_checkBoxes.size(); ++i) {
            int bit = m_items[i].bit;
            Qt::CheckState state = m_checkBoxes[i]->checkState();

            if (state == Qt::PartiallyChecked) {
                continue; // 半选状态保留原样
            }

            for (quint32* maskRef : m_maskRefs) {
                if (state == Qt::Checked) {
                    *maskRef |= (1U << bit);
                } else if (state == Qt::Unchecked) {
                    *maskRef &= ~(1U << bit);
                }
            }
        }
    }

    const QList<Item> m_items;
    QList<quint32*> m_maskRefs; 
    QList<QCheckBox*> m_checkBoxes;
};