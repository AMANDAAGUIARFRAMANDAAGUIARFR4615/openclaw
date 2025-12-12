#include "DeviceWindow.h"
#include "DeviceWidget.h"
#include "Logger.h"
#include "EventHub.h"
#include "ToastWidget.h"
#include "Tools.h"
#include "LiveStreamDevice.h"
#include "UsbDeviceManager.h"
#include "EmojiIconProvider.h"
#include "KeyMapping.h"
#include <QElapsedTimer>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QClipboard>
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
    connect(timer, &QTimer::timeout, [=]() {
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

QPoint DeviceWindow::getTransformedPosition(QPoint pos) {
    int x = pos.x() / deviceInfo->scaleFactor;
    int y = pos.y() / deviceInfo->scaleFactor;
    int w = videoFrameWidget->width() / deviceInfo->scaleFactor;
    int h = videoFrameWidget->height() / deviceInfo->scaleFactor;

    switch (deviceInfo->orientation) {
        case 1: // Portrait
            return QPoint(qBound(0, x, w), qBound(0, y, h));
        case 2: // PortraitUpsideDown
            return QPoint(qBound(0, w - x, w), qBound(0, h - y, h));
        case 3: // LandscapeRight (注意：X限制为h, Y限制为w)
            return QPoint(qBound(0, h - y, h), qBound(0, x, w));
        case 4: // LandscapeLeft (注意：X限制为h, Y限制为w)
            return QPoint(qBound(0, y, h), qBound(0, w - x, w));
        default:
            return QPoint(qBound(0, x, w), qBound(0, y, h));
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

bool DeviceWindow::event(QEvent *event)
{
    auto mouseEvent = dynamic_cast<QMouseEvent *>(event);
    if (!mouseEvent)
        return DeviceView::event(event);

    int type = 0;

    switch (event->type())
    {
    case QEvent::MouseButtonPress:
        type = 1;
        break;
    case QEvent::MouseButtonRelease:
        type = 2;
        break;
    case QEvent::MouseMove:
        type = 3;
        break;
    }

    if (type == 1)
        pressedButtons |= mouseEvent->button();

    if (type != 0 && (pressedButtons & Qt::LeftButton)) {
        QPoint globalPos = mapToGlobal(mouseEvent->pos());
        QPoint localPos = videoFrameWidget->mapFromGlobal(globalPos);
        auto pos = getTransformedPosition(localPos);

        QJsonObject dataObject;
        dataObject["type"] = type;
        dataObject["x"] = pos.x();
        dataObject["y"] = pos.y();

        connection->send("mouse", dataObject);
    }

    if (type == 2)
        pressedButtons &= ~mouseEvent->button();

    return DeviceView::event(event);
}

void DeviceWindow::keyPressEvent(QKeyEvent *event)
{
    DeviceView::keyPressEvent(event);

    if (event->key() == Qt::Key_Escape)
    {
        close();
        return;
    }

    const int key = event->key();
    const auto modifiers = event->modifiers();

    if (modifiers == Qt::NoModifier || modifiers == Qt::ShiftModifier) {
#ifdef _WIN32
        char c = KeyMapping::toChar(event->nativeScanCode());
#else
        char c = KeyMapping::toChar(event->nativeVirtualKey());
#endif

        if (c != 0) {
            qDebugEx() << "扫描码映射字符" << c;
            connection->send("keyboard", QJsonObject{{"type", "keyPress"}, {"key", QString(modifiers == Qt::ShiftModifier ? "Shift+" : "") + c}, {"repeat", event->isAutoRepeat()}});
            return;
        }

        if (key >= Qt::Key_A && key <= Qt::Key_Z) {
            connection->send("keyboard", QJsonObject{{"type", "keyPress"}, {"key", event->text()}, {"repeat", event->isAutoRepeat()}});
            return;
        }
    }

    if (event->matches(QKeySequence::Paste)) {
        const QMimeData *mimeData = qApp->clipboard()->mimeData();

        auto content = mimeData->text();

        if (content.startsWith("file://")) {
#ifdef Q_OS_WIN
            auto filePath = content.mid(8); // Windows 去掉 file:///
#else
            auto filePath = content.mid(7); // Mac/Linux 去掉 file://
#endif

            QFileInfo fileInfo(filePath);
            if (fileInfo.exists() && fileInfo.isFile()) {
                QImageReader reader(filePath);
                reader.setDecideFormatFromContent(true);

                QByteArray format = reader.format();
                if (format.isEmpty()) {
                    qCriticalEx() << "不是有效的图片文件 ->" << filePath;
                    return;
                }

                qDebugEx() << "检测到图片格式:" << format;

                QFile file(filePath);
                if (!file.open(QIODevice::ReadOnly)) {
                    qCriticalEx() << "无法打开文件读取 ->" << filePath;
                    return;
                }

                QByteArray fileData = file.readAll();
                file.close();

                connection->send("clipboard", QJsonObject{{"type", 2}, {"content", QString(fileData.toBase64())}});
                return;
            }
        }

        if (mimeData->hasText())
        {
            qDebugEx() << "剪切板内容是文本:" << content;

            connection->send("clipboard", QJsonObject{{"type", 1}, {"content", content}});
        }
        else if (mimeData->hasImage())
        {
            QImage image = mimeData->imageData().value<QImage>();
            auto base64Data = Tools::imageToBase64(image);
            qDebugEx() << "剪切板内容是图片:" << base64Data.length();

            connection->send("clipboard", QJsonObject{{"type", 2}, {"content", base64Data}});
        }
        else
        {
            new ToastWidget("此类型暂不支持", this);
        }

        return;
    }

    const bool isModifier = key == Qt::Key_Control || key == Qt::Key_Shift || key == Qt::Key_Alt || key == Qt::Key_Meta;
    const QString keySequence = QKeySequence(isModifier ? modifiers : (modifiers | key)).toString();
    qDebugEx() << "按下" << keySequence;
    connection->send("keyboard", QJsonObject{{"type", "keyPress"}, {"key", keySequence}, {"repeat", event->isAutoRepeat()}});
}

void DeviceWindow::keyReleaseEvent(QKeyEvent *event)
{
    qDebugEx() << "放开" << QKeySequence(event->key()).toString();

    DeviceView::keyReleaseEvent(event);
}

void DeviceWindow::wheelEvent(QWheelEvent *event)
{
    if (QOperatingSystemVersion::current().type() == QOperatingSystemVersion::MacOS)
        return;

    qDebugEx() << "wheelEvent" << event;

    currentPos = event->position().toPoint();

    if (!event->pixelDelta().isNull()) {
        auto moveEvent = new QMouseEvent(
            QEvent::MouseMove,
            currentPos,
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::NoModifier
        );
        QApplication::postEvent(this, moveEvent);
        return;
    }

    accumulatedDelta += event->angleDelta().y();

    if (wheelTimer)
        return;

    wheelTimer = new QTimer(this);
    stepCount = 0;

    auto pressEvent = new QMouseEvent(
        QEvent::MouseButtonPress,
        currentPos,
        Qt::LeftButton,
        Qt::LeftButton,
        Qt::NoModifier
    );
    QApplication::postEvent(this, pressEvent);

    connect(wheelTimer, &QTimer::timeout, [=]() mutable {
        int stepDelta;

        if (stepCount == maxSteps - 1)
            stepDelta = accumulatedDelta;// 最后一步，直接把剩余全部加上
        else
            stepDelta = accumulatedDelta / (maxSteps - stepCount);

        currentPos += QPoint(0, stepDelta);
        accumulatedDelta -= stepDelta;
        stepCount++;

        auto moveEvent = new QMouseEvent(
            QEvent::MouseMove,
            currentPos,
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::NoModifier
        );
        QApplication::postEvent(this, moveEvent);

        if (stepCount >= maxSteps) {
            auto releaseEvent = new QMouseEvent(
                QEvent::MouseButtonRelease,
                currentPos,
                Qt::LeftButton,
                Qt::LeftButton,
                Qt::NoModifier
            );
            QApplication::postEvent(this, releaseEvent);

            wheelTimer->stop();
            wheelTimer->deleteLater();
            wheelTimer = nullptr;
            accumulatedDelta = 0;
        }
    });

    wheelTimer->start(30);
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
