#include "DeviceWindow.h"
#include "DeviceWidget.h"
#include "Logger.h"
#include "EventHub.h"
#include "ToastWidget.h"
#include "Tools.h"
#include "LiveStreamDevice.h"
#include "UsbDeviceManager.h"
#include "EmojiIconProvider.h"
#include <QElapsedTimer>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QMimeData>
#include <QHBoxLayout>
#include <QPushButton>
#include <QOperatingSystemVersion>
#include <QApplication>
#include <QSlider>
#include <QImageReader>

DeviceWindow::DeviceWindow(DeviceConnection* connection, DeviceInfo* deviceInfo) : DeviceView(connection, deviceInfo)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto buttonLayout = new QVBoxLayout();

    auto homeScreenButton = new QPushButton(EmojiIconProvider::createIcon("🏠"), "主屏幕", this);
    connect(homeScreenButton, &QPushButton::clicked, this, &DeviceView::onHomeScreenClicked);
    buttonLayout->addWidget(homeScreenButton);

    auto centerControllerButton = new QPushButton(EmojiIconProvider::createIcon("🎛️"), "控制中心", this);
    connect(centerControllerButton, &QPushButton::clicked, this, &DeviceView::onCenterControllerClicked);
    buttonLayout->addWidget(centerControllerButton);

    auto appSwitcherButton = new QPushButton(EmojiIconProvider::createIcon("↕️"), "应用切换", this);
    connect(appSwitcherButton, &QPushButton::clicked, this, &DeviceView::onAppSwitcherClicked);
    buttonLayout->addWidget(appSwitcherButton);

    auto killAllAppButton = new QPushButton(EmojiIconProvider::createIcon("🧹"), "清理应用", this);
    connect(killAllAppButton, &QPushButton::clicked, this, &DeviceView::onKillAllAppClicked);
    buttonLayout->addWidget(killAllAppButton);

    auto fileButton = new QPushButton(EmojiIconProvider::createIcon("📁"), "文件管理", this);
    connect(fileButton, &QPushButton::clicked, this, &DeviceView::onFileClicked);
    buttonLayout->addWidget(fileButton);

    auto recorderButton = new QPushButton(EmojiIconProvider::createIcon("⏺️"), "录屏", this);
    connect(recorderButton, &QPushButton::clicked, this, &DeviceView::onRecorderClicked);
    buttonLayout->addWidget(recorderButton);

    auto appListButton = new QPushButton(EmojiIconProvider::createIcon("📱"), "应用列表", this);
    connect(appListButton, &QPushButton::clicked, this, &DeviceView::onAppListClicked);
    buttonLayout->addWidget(appListButton);

    auto screenshotButton = new QPushButton(EmojiIconProvider::createIcon("📸"), "截图", this);
    connect(screenshotButton, &QPushButton::clicked, this, &DeviceView::onScreenshotClicked);
    buttonLayout->addWidget(screenshotButton);

    auto restartButton = new QPushButton(EmojiIconProvider::createIcon("🔄"), "重启", this);
    connect(restartButton, &QPushButton::clicked, this, &DeviceView::onRebootClicked);
    buttonLayout->addWidget(restartButton);

    QPushButton *lockButton = new QPushButton(EmojiIconProvider::createIcon("🔒"), "锁屏", this);
    connect(lockButton, &QPushButton::clicked, this, &DeviceView::onLockClicked);
    buttonLayout->addWidget(lockButton);

    QPushButton *unlockButton = new QPushButton(EmojiIconProvider::createIcon("🔓"), "解锁", this);
    connect(unlockButton, &QPushButton::clicked, this, &DeviceView::onUnlockClicked);
    buttonLayout->addWidget(unlockButton);

    auto volumeSlider = new QSlider(Qt::Horizontal, this);
    volumeSlider->setRange(0, 100);
    volumeSlider->setStyleSheet(R"(
        QSlider::groove:horizontal {
            border-radius: 4px;
            height: 8px;
            background: #e0e0e0;
        }
        QSlider::handle:horizontal {
            background: #4CAF50;
            border: none;
            width: 16px;
            height: 16px;
            border-radius: 8px;
            margin: -4px 0;
        }
        QSlider::handle:horizontal:hover {
            background: #45a049;
        }
        QSlider::sub-page:horizontal {
            background: #4CAF50;
            border-radius: 4px;
        }
    )");

    auto volumeLabel = new QLabel("音量：0%", this);
    volumeLabel->setAlignment(Qt::AlignCenter);

    auto volumeLayout = new QVBoxLayout();
    volumeLayout->addWidget(volumeSlider);
    volumeLayout->addWidget(volumeLabel);

    const QString& volumeKey = deviceInfo->deviceId + "/volume";

    connect(volumeSlider, &QSlider::valueChanged, [=](int value) {
        volumeLabel->setText(QString("音量：%1%").arg(value));
        settings.setValue(volumeKey, value);

        if (value == 0) {
            if (audioDeviceConnection) {
                g_usbDeviceManager->disconnectDevice(audioDeviceConnection);
                audioDeviceConnection = nullptr;
            }
            
            audioPlayer->stop();
            delete audioPlayer->sourceDevice();
            delete audioPlayer->audioOutput();
            delete audioPlayer;
            audioPlayer = nullptr;
        }
        else {
            if (!audioPlayer) {
                audioPlayer = new QMediaPlayer(this);
                audioPlayer->setAudioOutput(new QAudioOutput(this));

                connection->send("audioPort", 0);

                EventHub::on(this, "audioPort", [this](const QJsonValue &data, DeviceConnection* connection) {
                    if (this->connection != connection)
                        return;

                    EventHub::off(this, "audioPort");

                    auto port = data["port"].toInt();
                    qDebugEx() << "audioPort" << port;

                    if (connection->type == DeviceConnection::Usb)
                    {
                        auto device = new LiveStreamDevice(nullptr, 0, this);
                        audioPlayer->setSourceDevice(device);
                        audioPlayer->play();
                        
                        auto ctx = g_usbDeviceManager->getContext(connection);
                        audioDeviceConnection = g_usbDeviceManager->connectDevice(ctx->udid, port, [=](DeviceConnection* conn, const QByteArray& data) {
                            if (audioPlayer)
                                device->appendData(data);
                        });
                    }
                    else
                    {
                        // auto device = new LiveStreamDevice(deviceInfo->localIp, deviceInfo->videoPort, this);
                        // player->setSourceDevice(device);
                    }
                });
            }

            audioPlayer->audioOutput()->setVolume(value / 100.0f);
        }
    });

    // volumeSlider->setValue(settings.value(volumeKey, 0u).toInt());
    volumeLayout->deleteLater();

    // buttonLayout->addLayout(volumeLayout);

    buttonLayout->addStretch();

    QWidget *buttonContainer = new QWidget(this);
    buttonContainer->setLayout(buttonLayout);

    layout->addWidget(buttonContainer);

    setLayout(layout);

    EventHub::on(this, "lockedStatus", [this](const QJsonValue &data, DeviceConnection* connection) {
        if (this->connection != connection)
            return;

        auto locked = data.toBool();

        if (locked)
            addOverlay("设备已锁定");
        else
            overlay->hide();
    });

    auto title = windowTitle(); 

    auto timer = new QTimer(this);
    timer->callOnTimeout([=]() {
        if (!videoFrameWidget)
            return;

        auto ioDevice = videoFrameWidget->mediaPlayer->sourceDevice();
        auto device = qobject_cast<LiveStreamDevice*>(const_cast<QIODevice*>(ioDevice));
        this->setWindowTitle(title + " " + Tools::formatByteSize(device->speedBps()) + " / s");
    });
    timer->start(1000);
}

DeviceWindow::~DeviceWindow()
{
    EventHub::off(this, "lockedStatus");
    EventHub::off(this, "audioPort");

    g_usbDeviceManager->disconnectDevice(audioDeviceConnection);
    
    if (audioPlayer) {
        audioPlayer->stop();
        delete audioPlayer->sourceDevice();
        delete audioPlayer->audioOutput();
        delete audioPlayer;
    }
}

void DeviceWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    DeviceView::mouseDoubleClickEvent(event);

    qDebugEx() << "双击" << event->button();

    if (event->button() == Qt::LeftButton) {
        QMouseEvent *pressEvent = new QMouseEvent(QEvent::MouseButtonPress, event->pos(), event->button(), event->button(), event->modifiers());

        QMouseEvent *releaseEvent = new QMouseEvent(QEvent::MouseButtonRelease, event->pos(), event->button(), event->button(), event->modifiers());

        QApplication::postEvent(this, pressEvent);

        QTimer::singleShot(100, [=]() {
            QApplication::postEvent(this, releaseEvent);
        });
    }
}

void DeviceWindow::resizeEvent(QResizeEvent *event)
{
    if (videoFrameWidget) {
        overlay->move(videoFrameWidget->pos());
        overlay->resize(videoFrameWidget->size());
    }

    DeviceView::resizeEvent(event);
}

QMenu* DeviceWindow::createContextMenu()
{
    auto menu = DeviceView::createContextMenu();
    auto subMenu = menu->addMenu(EmojiIconProvider::createIcon("🎬"), "清晰度");
    subMenu->addAction("360p", [this]() {
        connection->send("setVideoQuality", 3);
    });
    subMenu->addAction("480p", [this]() {
        connection->send("setVideoQuality", 4);
    });
    subMenu->addAction("720p", [this]() {
        connection->send("setVideoQuality", 5);
    });
    return menu;
}
