#include "DeviceWidget.h"
#include "DeviceWindow.h"
#include "EventHub.h"
#include "MainWindow.h"
#include "Safe.h"
#include "Account.h"
#include "VideoFrameWidget.h"
#include "LiveStreamDevice.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QClipboard>
#include <QInputDialog>
#include <QTcpServer>

DeviceWidget::DeviceWidget(DeviceConnection* connection, DeviceInfo* deviceInfo): DeviceView(connection, deviceInfo)
{
    auto layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto topLayout = new QHBoxLayout;
    topLayout->setContentsMargins(5, 0, 5, 0);
    topLayout->setSpacing(0);

    auto deviceInfoLabel = new QLabel(deviceInfo->displayName(true), this);
    deviceInfoLabel->setAlignment(Qt::AlignCenter);
    deviceInfoLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    deviceInfoLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    deviceInfoLabel->setOpenExternalLinks(false);
    deviceInfoLabel->setContextMenuPolicy(Qt::NoContextMenu);

    connect(deviceInfoLabel, &QLabel::linkActivated, [=](const QString &link){
        bool ok;

        QString newName = QInputDialog::getText(this, "重命名", "新名称：", QLineEdit::Normal, deviceInfo->deviceName, &ok);

        if (ok) {
            connection->send("deviceName", newName);
            deviceInfo->deviceName = newName;
            deviceInfoLabel->setText(deviceInfo->displayName(true));
            const auto item = property("listWidgetItem").value<QListWidgetItem*>();
            item->setText(deviceInfo->deviceName);
        }
    });

    EventHub::on(this, "deviceNameChanged", [=](const QJsonValue &data, DeviceConnection* connection) {
        if (this->connection != connection)
            return;

        deviceInfo->deviceName = data.toString();
        deviceInfoLabel->setText(deviceInfo->displayName(true));
        const auto item = property("listWidgetItem").value<QListWidgetItem*>();
        item->setText(deviceInfo->deviceName);
    });

    auto launchButton = new QPushButton(this);
    launchButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarMaxButton));
    launchButton->setFlat(true);
    launchButton->setToolTip("在独立窗口中打开");

    connect(launchButton, &QPushButton::clicked, this, &DeviceWidget::launchDeviceWindow);

    topLayout->addWidget(checkBox);
    topLayout->addStretch();
    topLayout->addWidget(deviceInfoLabel);
    topLayout->addStretch();
    topLayout->addWidget(launchButton);

    auto ipLabel = new QLabel(connection->type == DeviceConnection::Usb ? "" : deviceInfo->localIp, this);
    ipLabel->setObjectName("ipLabel");
    ipLabel->setAlignment(Qt::AlignCenter);
    ipLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    ipLabel->setFixedHeight(22);
    auto versionLabel = new QLabel(QStringList{"", "有根", "无根", "隐根"}[deviceInfo->jbType] + deviceInfo->version, this);
    versionLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    versionLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    versionLabel->setFixedHeight(22);

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

    addContextMenuActions();

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

    EventHub::on(this, HIDE_STR("deviceExpired"), [=](const QJsonValue &data, DeviceConnection* connection) {
        if (this->connection != connection)
            return;

        if (data.toBool() && deviceInfo->expireAt.get() > Account::getInstance()->loginTime.get() + elapsedTimer->elapsed()) {
            deviceInfo->expireAt = 1;
            DeviceInfo::expirations[deviceInfo->deviceId] = deviceInfo->expireAt;
        }
    });
}

DeviceWidget::~DeviceWidget()
{
    EventHub::off(this, "deviceNameChanged");
    EventHub::off(this, "lockedStatus");

    if (deviceWindow)
        deviceWindow->deleteLater();

    delete deviceInfo;
}

void DeviceWidget::setupVideoConnection()
{
    auto ipLabel = findChild<QLabel*>("ipLabel");

    auto device = new LiveStreamDevice(this);

    if (connection->type == DeviceConnection::Usb)
    {
        auto videoConnection = UsbDeviceManager::getInstance()->connectDevice(deviceInfo->deviceId, deviceInfo->videoPort, true);

        connect(UsbDeviceManager::getInstance(), &UsbDeviceManager::rawDataReceived, this, [=](DeviceConnection* sender, const QByteArray& data){
            if (sender != videoConnection)
                return;

            qint64 expireTime = deviceInfo->expireAt.get();
            qint64 currentTime = Account::getInstance()->loginTime.get() + elapsedTimer->elapsed();
            if (expireTime > currentTime)
            {
                device->appendData(data);

                if (expireTime - currentTime < HIDE_NUM(86400000))
                    ipLabel->setText(deviceInfo->localIp + HIDE_STR("<font color='orange'>[即将过期]</font>"));
                else
                    ipLabel->setText(deviceInfo->localIp);
            }
            else
            {
                ipLabel->setText(deviceInfo->localIp + (deviceInfo->expireAt.get() == 0 ? "" : HIDE_STR("<font color='red'>[已过期]</font>")));
            }
        });

        setSourceDevice(device);
    }
    else
    {
        auto server = new QTcpServer(this);
        connect(server, &QTcpServer::newConnection, [=]() {
            QTcpSocket *socket = server->nextPendingConnection();
            qDebugEx() << "投屏连接" << socket->peerAddress().toString();
            connect(socket, &QTcpSocket::readyRead, [=]() {
                const auto& data = socket->readAll();

                qint64 expireTime = deviceInfo->expireAt.get();
                qint64 currentTime = Account::getInstance()->loginTime.get() + elapsedTimer->elapsed();
                if (expireTime > currentTime)
                {
                    device->appendData(data);

                    if (expireTime - currentTime < HIDE_NUM(86400000))
                        ipLabel->setText(deviceInfo->localIp + HIDE_STR("<font color='orange'>[即将过期]</font>"));
                    else
                        ipLabel->setText(deviceInfo->localIp);
                }
                else
                {
                    ipLabel->setText(deviceInfo->localIp + (deviceInfo->expireAt.get() == 0 ? "" : HIDE_STR("<font color='red'>[已过期]</font>")));
                }
            });
        });
        server->listen(QHostAddress::Any, 0);
        connection->send("videoPort", server->serverPort());
        setSourceDevice(device);
    }
}

QByteArray DeviceWidget::grabFrame()
{
    return deviceWindow ? deviceWindow->getVideoFrameWidget()->grabFrame() : videoFrameWidget->grabFrame();
}

bool DeviceWidget::event(QEvent *event)
{
    if (deviceWindow)
        return qApp->sendEvent(deviceWindow, event);

    return DeviceView::event(event);
}

void DeviceWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (AppSettingsDialog::getInstance()->getValue("doubleClickOpenStandalone")) {
            launchDeviceWindow();
            return;
        }
    }

    DeviceView::mouseDoubleClickEvent(event);
}

void DeviceWidget::launchDeviceWindow() {
    static bool isDispatching = false;

    if (!isDispatching && MainWindow::getInstance()->multiControlSwitchButton->isChecked()) {
        isDispatching = true;
        for (auto& item : MainWindow::getInstance()->getDeviceWidgets(this)) {
            if (item != this)
                item->launchDeviceWindow();
        }
        isDispatching = false;
    }

    if (deviceWindow) {
        deviceWindow->setWindowState(deviceWindow->windowState() & ~Qt::WindowMinimized);
        deviceWindow->raise();
        deviceWindow->activateWindow();
        return;
    }

    if (overlay->isVisible())
    {
        QToolTip::showText(QCursor::pos(), "请先解锁");
        return;
    }

    showOverlay("设备控制中");

    auto videoFrameWidgetLocal = videoFrameWidget;
    auto placeholder = new QWidget();

    deviceWindow = new DeviceWindow(connection, deviceInfo, this);
    connect(deviceWindow, &QObject::destroyed, this, [=]() {
        addVideoFrameWidget(videoFrameWidgetLocal);

        deviceInfo->geometry = deviceWindow->geometry();
        settings->setValue(deviceInfo->deviceId + "/geometry", deviceInfo->geometry);
        deviceWindow = nullptr;
        placeholder->deleteLater();
    });

    if (deviceInfo->geometry.isValid()) {
        QRect targetRect = deviceInfo->geometry;
        bool isVisible = false;

        // 遍历所有屏幕，检查目标矩形是否与任意屏幕的可用区域相交
        for (QScreen *screen : qApp->screens()) {
            if (targetRect.intersects(screen->availableGeometry())) {
                isVisible = true;
                break; // 只要与任意一个屏幕有交集，就认为位置有效，停止检查
            }
        }

        // 如果遍历完所有屏幕，发现都不相交（说明窗口完全跑到了屏幕外）
        if (!isVisible) {
            // 强制移动到主屏幕中心
            // 注意：这里重置的是中心点，保持窗口大小不变
            // 如果窗口本身比屏幕还大，可能需要额外处理 resize，但通常 moveCenter 足够
            targetRect.moveCenter(qApp->primaryScreen()->availableGeometry().center());
        }

        deviceWindow->setGeometry(targetRect);
    }
    else {
        float limitW = qApp->primaryScreen()->availableSize().width();
        float limitH = qApp->primaryScreen()->availableSize().height();
        float scale = std::min({1.0f, limitW / deviceInfo->screenWidth, limitH / deviceInfo->screenHeight});
        deviceWindow->setFixedSize(deviceInfo->screenWidth * scale, deviceInfo->screenHeight * scale);
    }

    deviceWindow->addVideoFrameWidget(videoFrameWidget);
    deviceWindow->show();
    QTimer::singleShot(0, [=]() {
        // 多个窗口同时打开需要额外设置才不会被挡住
        deviceWindow->raise();
        deviceWindow->activateWindow();
    });
    qobject_cast<QBoxLayout*>(layout())->addWidget(placeholder);

    videoFrameWidget = nullptr;
}
