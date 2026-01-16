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
                                 quint32& maskRef,
                                 QWidget* parent = nullptr)
        : QDialog(parent)
        , m_items(items)
        , m_maskRef(maskRef)
    {
        setWindowTitle("位掩码编辑器");
        setModal(true);
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
            cb->setChecked(m_maskRef & (1U << item.bit));
            
            if (!item.bit) {
                cb->setChecked(true);
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
        m_maskRef = 0;

        for (int i = 0; i < m_checkBoxes.size(); ++i) {
            if (m_checkBoxes[i]->isChecked()) {
                int bit = m_items[i].bit;
                m_maskRef |= (1U << bit);
            }
        }
    }

    const QList<Item> m_items;
    quint32& m_maskRef;
    QList<QCheckBox*> m_checkBoxes;
};
