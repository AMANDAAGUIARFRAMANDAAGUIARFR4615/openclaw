#include "DeviceWidget.h"
#include "DeviceWindow.h"
#include "EventHub.h"
#include "ToastWidget.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QClipboard>
#include <QMouseEvent>

DeviceWidget::DeviceWidget(DeviceConnection* connection, DeviceInfo* deviceInfo): DeviceView(connection, deviceInfo)
{
    auto layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto deviceInfoLabel = new QLabel(connection->displayName(), this);
    deviceInfoLabel->setAlignment(Qt::AlignCenter);
    deviceInfoLabel->setFixedHeight(24);
    auto ipLabel = new QLabel(connection->type == DeviceConnection::Usb ? "" : deviceInfo->localIp, this);
    ipLabel->setAlignment(Qt::AlignCenter);
    ipLabel->setFixedHeight(24);
    auto versionLabel = new QLabel("版本：" + deviceInfo->version, this);
    versionLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    versionLabel->setFixedHeight(24);

    auto hLayout = new QHBoxLayout;
    hLayout->setContentsMargins(5, 0, 5, 0);
    hLayout->setSpacing(0);

    hLayout->addStretch();
    hLayout->addWidget(ipLabel);
    hLayout->addStretch();
    hLayout->addWidget(versionLabel);

    layout->addWidget(deviceInfoLabel);
    layout->addWidget(overlay);
    layout->addLayout(hLayout);
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

void DeviceWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    DeviceView::mouseDoubleClickEvent(event);

    qDebugEx() << "双击" << event->button();

    if (event->button() == Qt::LeftButton) {
        QApplication::postEvent(this, new QMouseEvent(QEvent::MouseButtonPress, event->pos(), event->button(), event->button(), event->modifiers()));
        QApplication::postEvent(this, new QMouseEvent(QEvent::MouseButtonRelease, event->pos(), event->button(), event->button(), event->modifiers()));
    }
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

    auto videoFrameWidgetLocal = videoFrameWidget;
    QPointer<QWidget> placeholder = new QWidget();

    deviceWindow = new DeviceWindow(connection, deviceInfo);
    connect(deviceWindow, &QObject::destroyed, [=]() {
        if (!placeholder)
            return;

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
