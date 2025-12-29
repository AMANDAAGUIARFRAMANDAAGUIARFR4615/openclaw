#pragma once

#include "global.h"
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QClipboard>

class AccountListDialog : public QDialog {
public:
    AccountListDialog(const QStringList &numbers, QWidget *parent = nullptr) : QDialog(parent) {
        setModal(true);
        setWindowTitle("在线账号列表");
        setMinimumWidth(300);
        
        auto mainLayout = new QVBoxLayout(this);
        auto scrollArea = new QScrollArea(this);
        auto scrollWidget = new QWidget();
        auto listLayout = new QVBoxLayout(scrollWidget);

        for (const QString &phone : numbers) {
            auto rowWidget = new QWidget();
            auto rowLayout = new QHBoxLayout(rowWidget);
            
            auto label = new QLabel(phone);
            auto btnScreenshot = new QPushButton("截图");
            
            rowLayout->addWidget(label);
            rowLayout->addStretch();
            rowLayout->addWidget(btnScreenshot);
            
            connect(btnScreenshot, &QPushButton::clicked, [=]() {
                webSocketClient->emitEvent("screenshot", phone, [=](const QJsonValue &res) {
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
            
            listLayout->addWidget(rowWidget);
        }

        scrollWidget->setLayout(listLayout);
        scrollArea->setWidget(scrollWidget);
        scrollArea->setWidgetResizable(true);
        mainLayout->addWidget(scrollArea);
    }
};
