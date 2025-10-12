#include "DeviceWidget.h"
#include "DeviceWindow.h"
#include "EventHub.h"
#include "ToastWidget.h"
#include <QVBoxLayout>
#include <QLabel>

DeviceWidget::DeviceWidget(QTcpSocket* socket, DeviceInfo* deviceInfo): DeviceView(socket, deviceInfo)
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

    EventHub::StartListening("lockedStatus", [this](const QJsonValue &data, QTcpSocket* socket) {
        if (this->socket != socket)
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

    deviceWindow = new DeviceWindow(socket, deviceInfo, this);
    videoFrameWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    videoFrameWidget->setFixedSize(deviceInfo->screenWidth * deviceInfo->scaleFactor, deviceInfo->screenHeight * deviceInfo->scaleFactor);
    videoFrameWidget->orientationChanged(deviceInfo->orientation);
    deviceWindow->setAttribute(Qt::WA_DeleteOnClose);
    deviceWindow->show();

    videoFrameWidget = nullptr;
    addOverlay("设备控制中");
}
