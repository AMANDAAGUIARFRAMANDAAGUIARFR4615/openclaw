#pragma once

#include "global.h"
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QClipboard>
#include <QComboBox>
#include <QJsonArray>
#include <QJsonObject>

class AccountListDialog : public QDialog {
public:
    AccountListDialog(const QStringList &numbers, QWidget *parent) : QDialog(parent) {
        setModal(true);
        setWindowTitle("在线账号列表");
        setMinimumWidth(600);
        
        auto mainLayout = new QVBoxLayout(this);
        auto scrollArea = new QScrollArea(this);
        auto scrollWidget = new QWidget();
        auto listLayout = new QVBoxLayout(scrollWidget);

        for (const QString &phone : numbers) {
            auto rowWidget = new QWidget();
            auto rowLayout = new QHBoxLayout(rowWidget);
            
            auto label = new QLabel(phone);
            label->setMinimumWidth(100);

            auto deviceComboBox = new QComboBox();
            deviceComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
            deviceComboBox->setPlaceholderText("请刷新获取设备");
            
            auto refreshDeviceButton = new QPushButton("刷新设备");

            auto screenshotButton = new QPushButton("截图");
            auto getLogButton = new QPushButton("日志");
            
            connect(refreshDeviceButton, &QPushButton::clicked, [=]() {
                deviceComboBox->clear();
                deviceComboBox->setPlaceholderText("正在加载...");
                
                webSocketClient->emitEvent("online_devices", phone, [=](const QJsonValue &res) {
                    if (res.isString()) {
                        deviceComboBox->setPlaceholderText(res.toString());
                        return;
                    }

                    const auto& devices = res.toArray();
                    if (devices.isEmpty()) {
                        deviceComboBox->setPlaceholderText("当前无在线设备");
                        return;
                    }

                    deviceComboBox->addItem("主界面", "");
                    
                    for (const QJsonValue &item : devices) {
                        const auto& deviceId = item["deviceId"].toString();
                        const auto& deviceName = item["deviceName"].toString();
                        const auto& model = item["model"].toString();
                        const auto& lockedStatus = item["lockedStatus"].toBool();
                        deviceComboBox->addItem(QString("%1 [%2]%3").arg(deviceName, model, lockedStatus ? " 锁屏" : ""), deviceId);
                    }

                    deviceComboBox->setCurrentIndex(0);
                });
            });

            rowLayout->addWidget(label);
            rowLayout->addWidget(deviceComboBox);
            rowLayout->addWidget(refreshDeviceButton);
            rowLayout->addStretch();
            rowLayout->addWidget(screenshotButton);
            rowLayout->addWidget(getLogButton);
            
            connect(screenshotButton, &QPushButton::clicked, [=]() {
                webSocketClient->emitEvent("screenshot", QJsonObject{{"phone", phone}, {"udid", deviceComboBox->currentData().toString()}}, [=](const QJsonValue &res) {
                    if (res.isString()) {
                        new ToastWidget(res.toString(), this);
                        return;
                    }

                    const auto& byteArray = QByteArray::fromBase64(res["base64"].toString().toUtf8());
                    QImage image;
                    
                    if (!image.loadFromData(byteArray))
                    {
                        new ToastWidget("图片数据解码失败", this);
                        return;
                    }

                    qApp->clipboard()->setPixmap(QPixmap::fromImage(image));
                    new ToastWidget("图片已复制到剪切板", this);
                });
            });

            connect(getLogButton, &QPushButton::clicked, [=]() {
                webSocketClient->emitEvent("get_log", phone, [=](const QJsonValue &res) {
                    if (res.isString()) {
                        new ToastWidget(res.toString(), this);
                        return;
                    }

                    qApp->clipboard()->setText(res["text"].toString());
                    new ToastWidget("文本已复制到剪切板", this);
                });
            });
            
            listLayout->addWidget(rowWidget);
        }

        scrollWidget->setLayout(listLayout);
        scrollArea->setWidget(scrollWidget);
        scrollArea->setWidgetResizable(true);
        mainLayout->addWidget(scrollArea);
    }
};
