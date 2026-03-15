#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QJsonDocument>
#include <QGuiApplication>
#include <QScreen>

class QrConnectDialog : public QDialog {
    Q_OBJECT

public:
    explicit QrConnectDialog(QWidget *parent = nullptr) : QDialog(parent) {
        setWindowTitle("用手机APP扫码连接");

#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
        setWindowState(Qt::WindowMaximized);
        auto mainLayout = new QVBoxLayout(this);
        mainLayout->setAlignment(Qt::AlignCenter);
#else
        auto mainLayout = new QHBoxLayout(this);
        mainLayout->setSizeConstraint(QLayout::SetFixedSize); 
#endif

        auto localIPs = NetworkUtils::getPhysicalIPs();
        qInfoEx() << "本机内网IP:" << localIPs;

        for (const QString &localIP : std::as_const(localIPs)) {
            const auto& hostInfo = TcpServer::getInstance()->getHostInfo(localIP);
            const auto& data = QJsonDocument(hostInfo).toJson(QJsonDocument::Compact).toBase64();
            
#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
            int screenW = qApp->primaryScreen()->availableGeometry().width();
            int qrSize = qMin(250, screenW - 100); 
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

            auto imgLabel = new QLabel(itemWidget);
            imgLabel->setPixmap(pixmap);
            imgLabel->setAlignment(Qt::AlignCenter);

            auto textLabel = new QLabel(localIP, itemWidget);
            textLabel->setAlignment(Qt::AlignCenter);
            auto font = textLabel->font();
            font.setPointSize(16);
            font.setBold(true);
            textLabel->setFont(font);

            itemLayout->addWidget(imgLabel);
            itemLayout->addWidget(textLabel);

            mainLayout->addWidget(itemWidget);
        }

        if (localIPs.isEmpty()) {
            auto errLabel = new QLabel("未检测到有效网卡", this);
            mainLayout->addWidget(errLabel);
        }

#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
        auto closeButton = new QPushButton("关闭", this);
        closeButton->setMinimumHeight(50);
        connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
        mainLayout->addWidget(closeButton);
#endif
    }

    ~QrConnectDialog() override = default;
};