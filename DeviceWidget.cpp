#include "DeviceWidget.h"
#include "DeviceWindow.h"
#include "EventHub.h"
#include "ToastWidget.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QApplication>
#include <QClipboard>

DeviceWidget::DeviceWidget(DeviceConnection* connection, DeviceInfo* deviceInfo): DeviceView(connection, deviceInfo)
{
    auto layout = new QVBoxLayout;

    auto deviceInfoLabel = new QLabel(connection->displayName(), this);
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

    EventHub::on(this, "clipboard", [this](const QJsonValue &data, DeviceConnection* connection) {
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

    EventHub::on(this, "lockedStatus", [=](const QJsonValue &data, DeviceConnection* connection) {
        if (this->connection != connection)
            return;

        auto locked = data.toBool();
        deviceInfo->lockedStatus = locked;

        if (deviceWindow)
            return;

        if (locked)
            addOverlay("设备已锁定");
        else
            overlay->hide();
    });
}

DeviceWidget::~DeviceWidget()
{
    EventHub::off(this, "clipboard");
    EventHub::off(this, "lockedStatus");
}

void DeviceWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    DeviceView::mouseDoubleClickEvent(event);

    if (deviceWindow) {
        deviceWindow->activateWindow();
        return;
    }

    if (overlay->isVisible())
    {
        new ToastWidget("需要先解锁才能控制", this);
        return;
    }

    addOverlay("设备控制中");

    videoFrameWidgetSize = videoFrameWidget->size();

    auto placeholder = new QWidget();

    deviceWindow = new DeviceWindow(connection, deviceInfo, this);
    connect(deviceWindow, &QObject::destroyed, [=]() {
        deviceWindow = nullptr;
        placeholder->deleteLater();
    });

    videoFrameWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    videoFrameWidget->setFixedSize(deviceInfo->screenWidth * deviceInfo->scaleFactor, deviceInfo->screenHeight * deviceInfo->scaleFactor);
    deviceWindow->addVideoFrameWidget(videoFrameWidget);
    deviceWindow->show();
    qobject_cast<QBoxLayout*>(layout())->addWidget(placeholder);

    videoFrameWidget = nullptr;
}
