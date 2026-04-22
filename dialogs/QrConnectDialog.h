#pragma once

#include "BaseDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QJsonDocument>
#include <QGuiApplication>
#include <QScreen>

class QrConnectDialog : public BaseDialog {
    Q_OBJECT

public:
    explicit QrConnectDialog(QWidget *parent = nullptr) : BaseDialog("用手机APP扫码连接", parent) {
        QBoxLayout *mainLayout = contentLayout();
        auto localIPs = NetworkUtils::getPhysicalIPs();
        qInfoEx() << "本机内网IP:" << localIPs;

#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
        mainLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
        mainLayout->setSpacing(12);
        const int screenW = qApp->primaryScreen()->availableGeometry().width();
        const int qrMaxByScreen = qMax(140, screenW - 96);
#endif

        for (const QString &localIP : std::as_const(localIPs)) {
            const auto& hostInfo = TcpServer::getInstance()->getHostInfo(localIP);
            const auto& data = QJsonDocument(hostInfo).toJson(QJsonDocument::Compact).toBase64();
            
#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
            int qrSize = localIPs.size() > 1 ? qMin(200, qrMaxByScreen) : qMin(250, qrMaxByScreen);
#else
            int qrSize = qMax(200, 500 - (localIPs.size() * 100));
#endif

            auto img = Tools::generateQrImage(data);
            QPixmap pixmap = QPixmap::fromImage(img).scaled(
                qrSize, qrSize, 
                Qt::KeepAspectRatio, 
                Qt::SmoothTransformation
            );

            auto itemWidget = new QWidget(this);
            auto itemLayout = new QVBoxLayout(itemWidget);
            itemLayout->setContentsMargins(0, 0, 0, 0);
#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
            itemLayout->setSpacing(8);
#else
            itemLayout->setSpacing(12);
#endif

            auto imgLabel = new QLabel(itemWidget);
            imgLabel->setPixmap(pixmap);
            imgLabel->setAlignment(Qt::AlignCenter);

            auto textLabel = new QLabel(localIP, itemWidget);
            textLabel->setAlignment(Qt::AlignCenter);
            auto font = textLabel->font();
#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
            font.setPointSize(13);
#else
            font.setPointSize(16);
#endif
            font.setBold(true);
            textLabel->setFont(font);

            itemLayout->addWidget(imgLabel);
            itemLayout->addWidget(textLabel);

            mainLayout->addWidget(itemWidget);
        }

        if (localIPs.isEmpty()) {
            auto errLabel = new QLabel("未检测到有效网卡", this);
            errLabel->setAlignment(Qt::AlignCenter);
            mainLayout->addWidget(errLabel);
        }
    }

    ~QrConnectDialog() override = default;
};