#include "DeviceWidget.h"
#include "DeviceWindow.h"
#include "EventHub.h"
#include "ToastWidget.h"
#include "MainWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QClipboard>
#include <QMouseEvent>

DeviceWidget::DeviceWidget(DeviceConnection* connection, DeviceInfo* deviceInfo): DeviceView(connection, deviceInfo)
{
    auto layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto topLayout = new QHBoxLayout;
    topLayout->setContentsMargins(5, 0, 5, 0);
    topLayout->setSpacing(0);

    auto deviceInfoLabel = new QLabel(connection->displayName(), this);
    deviceInfoLabel->setAlignment(Qt::AlignCenter);
    deviceInfoLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    deviceInfoLabel->setFixedHeight(24);

    auto launchButton = new QPushButton(this);
    launchButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarMaxButton));
    launchButton->setFlat(true);
    launchButton->setCursor(Qt::PointingHandCursor);
    launchButton->setToolTip("在独立窗口中打开");
    
    connect(launchButton, &QPushButton::clicked, this, &DeviceWidget::launchDeviceWindow);

    topLayout->addWidget(deviceInfoLabel);
    topLayout->addWidget(launchButton);

    auto ipLabel = new QLabel(connection->type == DeviceConnection::Usb ? "" : deviceInfo->localIp, this);
    ipLabel->setAlignment(Qt::AlignCenter);
    ipLabel->setFixedHeight(24);
    auto versionLabel = new QLabel("版本：" + deviceInfo->version, this);
    versionLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    versionLabel->setFixedHeight(24);

    auto bottomLayout = new QHBoxLayout;
    bottomLayout->setContentsMargins(5, 0, 5, 0);
    bottomLayout->setSpacing(0);

    bottomLayout->addStretch();
    bottomLayout->addWidget(ipLabel);
    bottomLayout->addStretch();
    bottomLayout->addWidget(versionLabel);

    layout->addLayout(topLayout); 
    layout->addWidget(overlay);
    layout->addLayout(bottomLayout);
    setLayout(layout);

    EventHub::on(this, "clipboard", [this](const QJsonValue &data, DeviceConnection* connection) {
        if (this->connection != connection)
            return;

        auto type = data["type"].toInt();
        auto content = data["content"].toString();

        if (type == 1)
        {
            qApp->clipboard()->setText(content);
            new ToastWidget("文本已复制到剪切板", this);
            return;
        }
        
        if (type == 2)
        {
            QByteArray byteArray = QByteArray::fromBase64(content.toUtf8());
            QImage image;
            
            if (!image.loadFromData(byteArray))
            {
                new ToastWidget("图片数据解码失败", this);
                return;
            }

            qApp->clipboard()->setPixmap(QPixmap::fromImage(image));
            new ToastWidget("图片已复制到剪切板", this);
            return;
        }

        new ToastWidget("此类型暂不支持", this);
    });

    EventHub::on(this, "lockedStatus", [=](const QJsonValue &data, DeviceConnection* connection) {
        if (this->connection != connection)
            return;

        auto locked = data.toBool();
        deviceInfo->lockedStatus = locked;

        if (deviceWindow)
            return;

        if (locked)
            showOverlay("设备已锁定");
        else
            hideOverlay();
    });

    EventHub::on(this, "orientation", [this](const QJsonValue &data, DeviceConnection* connection) {
        if (this->connection != connection)
            return;

        this->deviceInfo->orientation = data.toInt();
    });
}

DeviceWidget::~DeviceWidget()
{
    EventHub::off(this, "clipboard");
    EventHub::off(this, "lockedStatus");

    if (deviceWindow)
        deviceWindow->deleteLater();

    delete connection->deviceInfo;
}

void DeviceWidget::launchDeviceWindow() {
    if (deviceWindow) {
        deviceWindow->activateWindow();
        return;
    }

    if (overlay->isVisible())
    {
        new ToastWidget("需要先解锁才能控制", this);
        return;
    }

    showOverlay("设备控制中");

    auto tab = MainWindow::getInstance()->getTab();
    auto videoQuality = tab.getVideoQuality();
    connection->send("setVideoQuality", qMax(videoQuality, 3));

    auto videoFrameWidgetLocal = videoFrameWidget;
    QPointer<QWidget> placeholder = new QWidget();

    deviceWindow = new DeviceWindow(connection, deviceInfo);
    connect(deviceWindow, &QObject::destroyed, [=]() {
        if (!placeholder)
            return;

        connection->send("setVideoQuality", videoQuality);

        addVideoFrameWidget(videoFrameWidgetLocal);

        deviceWindow = nullptr;
        placeholder->deleteLater();
    });

    deviceWindow->setFixedSize(deviceInfo->screenWidth * deviceInfo->scaleFactor, deviceInfo->screenHeight * deviceInfo->scaleFactor);
    deviceWindow->addVideoFrameWidget(videoFrameWidget);
    deviceWindow->show();
    qobject_cast<QBoxLayout*>(layout())->addWidget(placeholder);

    videoFrameWidget = nullptr;
}
