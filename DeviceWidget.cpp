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
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto deviceInfoLabel = new QLabel(connection->displayName(), this);
    deviceInfoLabel->setAlignment(Qt::AlignCenter);
    deviceInfoLabel->setFixedHeight(24);
    auto ipLabel = new QLabel(deviceInfo->localIp, this);
    ipLabel->setAlignment(Qt::AlignCenter);
    ipLabel->setFixedHeight(24);
    
    layout->addWidget(deviceInfoLabel);
    layout->addWidget(overlay);
    layout->addWidget(ipLabel);
    setLayout(layout);

    EventHub::on(this, "clipboard", [this](const QJsonValue &data, DeviceConnection* connection) {
        if (this->connection != connection)
            return;

        auto type = data["type"].toInt();
        auto content = data["content"].toString();

        if (type == 1)
        {
            qApp->clipboard()->setText(content);
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

    if (deviceWindow)
        deviceWindow->deleteLater();

    delete connection->deviceInfo;
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

    auto videoFrameWidgetLocal = videoFrameWidget;
    auto videoFrameWidgetSize = videoFrameWidget->size();
    QPointer<QWidget> placeholder = new QWidget();

    deviceWindow = new DeviceWindow(connection, deviceInfo);
    connect(deviceWindow, &QObject::destroyed, [=]() {
        if (!placeholder)
            return;

        videoFrameWidgetLocal->setMinimumSize(0, 0);
        videoFrameWidgetLocal->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        videoFrameWidgetLocal->resize(videoFrameWidgetSize);
        addVideoFrameWidget(videoFrameWidgetLocal);

        deviceWindow = nullptr;
        placeholder->deleteLater();
    });

    videoFrameWidget->setFixedSize(deviceInfo->screenWidth * deviceInfo->scaleFactor, deviceInfo->screenHeight * deviceInfo->scaleFactor);
    deviceWindow->addVideoFrameWidget(videoFrameWidget);
    deviceWindow->show();
    qobject_cast<QBoxLayout*>(layout())->addWidget(placeholder);

    videoFrameWidget = nullptr;
}
