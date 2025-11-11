#include "DeviceWindow.h"
#include "DeviceWidget.h"
#include "Logger.h"
#include "TcpServer.h"
#include "EventHub.h"
#include "ToastWidget.h"
#include "Tools.h"
#include "LiveStreamDevice.h"
#include "UsbDeviceManager.h"
#include <QStyle>
#include <QElapsedTimer>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QClipboard>
#include <QMimeData>
#include <QHBoxLayout>
#include <QPushButton>
#include <QOperatingSystemVersion>
#include <QApplication>
#include <QAudioOutput>

DeviceWindow::DeviceWindow(DeviceConnection* connection, DeviceInfo* deviceInfo, DeviceWidget* deviceWidget) : DeviceView(connection, deviceInfo), deviceWidget(deviceWidget)
{
    setAttribute(Qt::WA_InputMethodEnabled, true);

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto buttonLayout = new QVBoxLayout();
    buttonLayout->setContentsMargins(5, 5, 5, 5);
    buttonLayout->setSpacing(5);

    auto homeScreenButton = new QPushButton(QIcon(":/icons/home.png"), "主屏幕", this);
    connect(homeScreenButton, &QPushButton::clicked, this, &DeviceView::onHomeScreenClicked);
    buttonLayout->addWidget(homeScreenButton);

    auto centerControllerButton = new QPushButton(QIcon(":/icons/dashboard.png"), "控制中心", this);
    connect(centerControllerButton, &QPushButton::clicked, this, &DeviceView::onCenterControllerClicked);
    buttonLayout->addWidget(centerControllerButton);

    auto appSwitcherButton = new QPushButton(QIcon(":/icons/flip_to_front.png"), "应用切换", this);
    connect(appSwitcherButton, &QPushButton::clicked, this, &DeviceView::onAppSwitcherClicked);
    buttonLayout->addWidget(appSwitcherButton);

    auto killAllAppButton = new QPushButton(QIcon(":/icons/kill.png"), "清理应用", this);
    connect(killAllAppButton, &QPushButton::clicked, this, &DeviceView::onKillAllAppClicked);
    buttonLayout->addWidget(killAllAppButton);

    auto fileButton = new QPushButton(QIcon(":/icons/file_move.png"), "文件管理", this);
    connect(fileButton, &QPushButton::clicked, this, &DeviceView::onFileClicked);
    buttonLayout->addWidget(fileButton);

    auto recorderButton = new QPushButton(QIcon(":/icons/screen_record.png"), "录屏", this);
    connect(recorderButton, &QPushButton::clicked, this, &DeviceView::onRecorderClicked);
    buttonLayout->addWidget(recorderButton);

    auto appListButton = new QPushButton(QIcon(":/icons/apps.png"), "应用列表", this);
    connect(appListButton, &QPushButton::clicked, this, &DeviceView::onAppListClicked);
    buttonLayout->addWidget(appListButton);

    auto screenshotButton = new QPushButton(QIcon(":/icons/screenshot.png"), "截图", this);
    connect(screenshotButton, &QPushButton::clicked, this, &DeviceView::onScreenshotClicked);
    buttonLayout->addWidget(screenshotButton);

    auto restartButton = new QPushButton(QIcon(":/icons/restart.png"), "重启", this);
    connect(restartButton, &QPushButton::clicked, this, &DeviceView::onRebootClicked);
    buttonLayout->addWidget(restartButton);

    QPushButton *unlockButton = new QPushButton(QIcon(":/icons/unlock.png"), "解锁", this);
    connect(unlockButton, &QPushButton::clicked, this, &DeviceView::onUnlockClicked);
    buttonLayout->addWidget(unlockButton);

    QPushButton *lockButton = new QPushButton(QIcon(":/icons/lock.png"), "锁屏", this);
    connect(lockButton, &QPushButton::clicked, this, &DeviceView::onLockClicked);
    buttonLayout->addWidget(lockButton);

    QPushButton *audioButton = new QPushButton(QIcon(":/icons/audio.png"), "音频", this);
    connect(audioButton, &QPushButton::clicked, this, [=]() {
        connection->send("audioPort", 0);
    });
    buttonLayout->addWidget(audioButton);

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

    EventHub::on(this, "audioPort", [this](const QJsonValue &data, DeviceConnection* connection) {
        if (this->connection != connection)
            return;

        auto port = data["port"].toInt();
        if (connection->type == DeviceConnection::Usb)
        {
            auto device = new LiveStreamDevice(nullptr, 0, this);
            
            auto manager = UsbDeviceManager::instance();
            auto ctx = manager->getContext(connection);
            auto deviceConnection = manager->connectDevice(ctx->udid, port, [=](DeviceConnection* conn, const QByteArray& data){
                device->appendData(data);
            });

            auto player = new QMediaPlayer(this);
            player->setAudioOutput(new QAudioOutput(this));

            player->setSourceDevice(device);
            player->play();

            connect(this, &QObject::destroyed, [=]() {
                deviceConnection->close();
            });
        }
        else
        {
            // auto device = new LiveStreamDevice(deviceInfo->localIp, deviceInfo->videoPort, this);
            // player->setSourceDevice(device);
        }
    });

    auto title = windowTitle(); 

    auto timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [=]() {
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
}

QPoint DeviceWindow::getTransformedPosition(QPoint pos) {
    auto x = pos.x() / deviceInfo->scaleFactor;
    auto y = pos.y() / deviceInfo->scaleFactor;
    auto width = this->width() / deviceInfo->scaleFactor;
    auto height = this->height() / deviceInfo->scaleFactor;

    switch (deviceInfo->orientation) {
        case 1: // Portrait
            return QPoint(x, y);
        case 2: // PortraitUpsideDown
            return QPoint(width - x, height - y);
        case 3: // LandscapeRight
            return QPoint(height - y, x);
        case 4: // LandscapeLeft
            return QPoint(y, width - x);
        default:
            return QPoint(x, y);
    }
}

void DeviceWindow::closeEvent(QCloseEvent *event)
{
    videoFrameWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    videoFrameWidget->setFixedSize(deviceWidget->videoFrameWidgetSize);
    deviceWidget->addVideoFrameWidget(videoFrameWidget);
    videoFrameWidget = nullptr;
    DeviceView::closeEvent(event);
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

    auto keySequence = QKeySequence(event->key()).toString();

    if (event->modifiers() & Qt::ControlModifier) {
        if (event->key() == Qt::Key_A || event->key() == Qt::Key_C || event->key() == Qt::Key_X || event->key() == Qt::Key_Z || event->key() == Qt::Key_Y || event->key() == Qt::Key_Space)
        {
            QJsonObject dataObject;
            dataObject["type"] = "keyPress";
            dataObject["key"] = QKeySequence(event->modifiers()).toString() + keySequence;

            connection->send("keyboard", dataObject);
            return;
        }

        if (event->key() == Qt::Key_V)
        {
            QClipboard *clipboard = QApplication::clipboard();
            const QMimeData *mimeData = clipboard->mimeData();

            QJsonObject dataObject;

            if (mimeData->hasText())
            {
                qDebugEx() << "剪切板内容是文本:" << mimeData->text();

                dataObject["type"] = 1;
                dataObject["content"] = mimeData->text();
            }
            else if (mimeData->hasImage())
            {
                QImage image = mimeData->imageData().value<QImage>();
                auto base64Data = Tools::imageToBase64(image);
                qDebugEx() << "剪切板内容是图片:" << base64Data.length();

                dataObject["type"] = 2;
                dataObject["content"] = base64Data;
            }
            else
            {
                new ToastWidget("此类型暂不支持", this);
                return;
            }

            connection->send("clipboard", dataObject);

            return;
        }
    }

    qDebugEx() << "Key Pressed:" << keySequence;

    QList<int> keys = {
        Qt::Key_Backspace, Qt::Key_Delete, Qt::Key_Enter, Qt::Key_Return,
        Qt::Key_Up, Qt::Key_Down, Qt::Key_Left, Qt::Key_Right,
        Qt::Key_Tab
    };

    if (keys.contains(event->key()))
    {
        QJsonObject dataObject;
        dataObject["type"] = "keyPress";
        dataObject["key"] = QKeySequence(event->modifiers()).toString() + keySequence;

        connection->send("keyboard", dataObject);
        return;
    }

    auto keyText = event->text();

    if (keyText.isEmpty())
        return;

    qDebugEx() << "按键输入:" << keyText;

    connection->send("inputText", keyText);
}

void DeviceWindow::keyReleaseEvent(QKeyEvent *event)
{
    auto keyText = QKeySequence(event->key()).toString();

    qDebugEx() << "Key Released:" << keyText;

    DeviceView::keyReleaseEvent(event);
}

void DeviceWindow::inputMethodEvent(QInputMethodEvent *event)
{
    QString commitText = event->commitString();
    if (!commitText.isEmpty())
    {
        qDebugEx() << "输入内容:" << commitText;
        connection->send("inputText", commitText);
    }

    DeviceView::inputMethodEvent(event);
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

    connect(wheelTimer, &QTimer::timeout, this, [=]() mutable {
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
