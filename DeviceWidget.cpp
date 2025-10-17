#include "DeviceWidget.h"
#include "DeviceWindow.h"
#include "EventHub.h"
#include "ToastWidget.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QClipboard>

DeviceWidget::DeviceWidget(DeviceConnection* connection, DeviceInfo* deviceInfo): DeviceView(connection, deviceInfo)
{
    auto layout = new QVBoxLayout;

    auto deviceInfoText = QString("%1 - %2  |  %3 x %4")
        .arg(deviceInfo->deviceName)
        .arg(deviceInfo->platform)
        .arg(deviceInfo->screenWidth)
        .arg(deviceInfo->screenHeight);

    auto deviceInfoLabel = new QLabel(deviceInfoText, this);
    deviceInfoLabel->setAlignment(Qt::AlignCenter);
    deviceInfoLabel->setStyleSheet(
        "font-size: 12px; "
        "font-weight: bold; "
        "padding: 5px; "
        "background-color: rgba(0, 0, 0, 50%); "
        "color: white; "
        "border-radius: 5px;");
    
    deviceInfoLabel->setFixedHeight(24);
    
    layout->addWidget(deviceInfoLabel);
    setLayout(layout);

    EventHub::StartListening("clipboard", [this](const QJsonValue &data, DeviceConnection* connection) {
        if (this->connection != connection)
            return;

        auto type = data["type"].toInt();
        auto content = data["content"].toString();

        QClipboard *clipboard = QApplication::clipboard();

        if (type == 1)
        {
            clipboard->setText(content);
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

            clipboard->setPixmap(QPixmap::fromImage(image));
            return;
        }

        new ToastWidget("此类型暂不支持", this);
    });

    EventHub::StartListening("lockedStatus", [this](const QJsonValue &data, DeviceConnection* connection) {
        if (this->connection != connection)
            return;

        if (deviceWindow)
            return;

        auto locked = data.toBool();

        if (locked)
            addOverlay("设备已锁定");
        else
            addVideoFrameWidget(new VideoFrameWidget(this));
    });
}

DeviceWidget::~DeviceWidget()
{

}

void DeviceWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    QWidget::mouseDoubleClickEvent(event);

    if (!videoFrameWidget)
    {
        new ToastWidget("需要先解锁才能控制", this);
        return;
    }

    videoFrameWidgetSize = videoFrameWidget->size();

    deviceWindow = new DeviceWindow(connection, deviceInfo, this);
    videoFrameWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    videoFrameWidget->setFixedSize(deviceInfo->screenWidth * deviceInfo->scaleFactor, deviceInfo->screenHeight * deviceInfo->scaleFactor);
    videoFrameWidget->orientationChanged(deviceInfo->orientation);
    deviceWindow->setAttribute(Qt::WA_DeleteOnClose);
    deviceWindow->show();

    videoFrameWidget = nullptr;
    addOverlay("设备控制中");
}
