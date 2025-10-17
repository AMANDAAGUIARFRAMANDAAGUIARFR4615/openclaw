#include "DeviceWindow.h"
#include "DeviceWidget.h"
#include "Logger.h"
#include "TcpServer.h"
#include "EventHub.h"
#include "ToastWidget.h"
#include "Tools.h"
#include <QStyle>
#include <QElapsedTimer>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QClipboard>
#include <QMimeData>
#include <QHBoxLayout>
#include <QPushButton>

DeviceWindow::DeviceWindow(DeviceConnection* connection, DeviceInfo* deviceInfo, DeviceWidget* deviceWidget) : DeviceView(connection, deviceInfo), deviceWidget(deviceWidget)
{
    setAttribute(Qt::WA_InputMethodEnabled, true);

    videoFrameWidget = deviceWidget->getVideoFrameWidget();
    mediaSource = deviceWidget->mediaSource;

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(videoFrameWidget);

    QVBoxLayout *buttonLayout = new QVBoxLayout();
    buttonLayout->setContentsMargins(5, 5, 5, 5);
    buttonLayout->setSpacing(5);

    QPushButton *homeScreenButton = new QPushButton(QIcon(":/icons/home.png"), "主屏幕", this);
    connect(homeScreenButton, &QPushButton::clicked, this, &DeviceView::onHomeScreenClicked);

    QPushButton *centerControllerButton = new QPushButton(QIcon(":/icons/dashboard.png"), "控制中心", this);
    connect(centerControllerButton, &QPushButton::clicked, this, &DeviceView::onCenterControllerClicked);

    QPushButton *appSwitcherButton = new QPushButton(QIcon(":/icons/flip_to_front.png"), "应用切换", this);
    connect(appSwitcherButton, &QPushButton::clicked, this, &DeviceView::onAppSwitcherClicked);

    QPushButton *killAllAppButton = new QPushButton(QIcon(":/icons/kill.png"), "清理应用", this);
    connect(killAllAppButton, &QPushButton::clicked, this, &DeviceView::onKillAllAppClicked);

    QPushButton *fileButton = new QPushButton(QIcon(":/icons/file_move.png"), "文件管理", this);
    connect(fileButton, &QPushButton::clicked, this, &DeviceView::onFileClicked);

    QPushButton *screenshotButton = new QPushButton(QIcon(":/icons/screenshot.png"), "截图", this);
    connect(screenshotButton, &QPushButton::clicked, this, &DeviceView::onScreenshotClicked);

    QPushButton *restartButton = new QPushButton(QIcon(":/icons/restart.png"), "重启", this);
    connect(restartButton, &QPushButton::clicked, this, &DeviceView::onRebootClicked);

    QPushButton *lockButton = new QPushButton(QIcon(":/icons/lock.png"), "锁屏", this);
    connect(lockButton, &QPushButton::clicked, this, &DeviceView::onLockClicked);

    QPushButton *unlockButton = new QPushButton(QIcon(":/icons/unlock.png"), "解锁", this);
    connect(unlockButton, &QPushButton::clicked, this, &DeviceView::onUnlockClicked);

    buttonLayout->addWidget(homeScreenButton);
    buttonLayout->addWidget(centerControllerButton);
    buttonLayout->addWidget(appSwitcherButton);
    buttonLayout->addWidget(killAllAppButton);
    buttonLayout->addWidget(fileButton);
    buttonLayout->addWidget(screenshotButton);
    buttonLayout->addWidget(restartButton);
    buttonLayout->addWidget(lockButton);
    buttonLayout->addWidget(unlockButton);
    buttonLayout->addStretch();

    QWidget *buttonContainer = new QWidget(this);
    buttonContainer->setLayout(buttonLayout);

    layout->addWidget(buttonContainer);

    setLayout(layout);

    EventHub::StartListening("lockedStatus", [this](const QJsonValue &data, DeviceConnection* connection) {
        if (this->connection != connection)
            return;

        auto locked = data.toBool();

        if (locked)
            addOverlay("设备已锁定");
        else
            addVideoFrameWidget(new VideoFrameWidget(this));
    });
}

DeviceWindow::~DeviceWindow()
{

}

void DeviceWindow::addOverlay(const QString &text)
{
    auto orientation = deviceInfo->orientation;
    auto size = videoFrameWidget->size();

    DeviceView::addOverlay(text);
    overlay->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    overlay->setFixedSize(size);
}

void DeviceWindow::addVideoFrameWidget(VideoFrameWidget* videoFrameWidget)
{
    videoFrameWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    videoFrameWidget->setFixedSize(deviceInfo->screenWidth * deviceInfo->scaleFactor, deviceInfo->screenHeight * deviceInfo->scaleFactor);
    DeviceView::addVideoFrameWidget(videoFrameWidget);
}

QPointF DeviceWindow::getTransformedPosition(QMouseEvent *event) {
    auto x = event->pos().x() / deviceInfo->scaleFactor;
    auto y = event->pos().y() / deviceInfo->scaleFactor;
    auto width = this->width() / deviceInfo->scaleFactor;
    auto height = this->height() / deviceInfo->scaleFactor;

    switch (deviceInfo->orientation) {
        case 1: // Portrait
            return QPointF(x, y);
        case 2: // PortraitUpsideDown
            return QPointF(width - x, height - y);
        case 3: // LandscapeRight
            return QPointF(height - y, x);
        case 4: // LandscapeLeft
            return QPointF(y, width - x);
        default:
            return QPointF(x, y);
    }
}

void DeviceWindow::closeEvent(QCloseEvent *event)
{
    if (!videoFrameWidget)
        return;

    videoFrameWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    videoFrameWidget->setFixedSize(deviceWidget->videoFrameWidgetSize);
    deviceWidget->addVideoFrameWidget(videoFrameWidget);
    videoFrameWidget = nullptr;
}

void DeviceWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    QWidget::mouseDoubleClickEvent(event);

    qDebugEx() << "双击" << event->button();

    if (event->button() == Qt::LeftButton) {
        QMouseEvent *pressEvent = new QMouseEvent(QEvent::MouseButtonPress, event->pos(), event->button(), event->button(), event->modifiers());

        QMouseEvent *releaseEvent = new QMouseEvent(QEvent::MouseButtonRelease, event->pos(), event->button(), event->button(), event->modifiers());

        QApplication::postEvent(this, pressEvent);

        QTimer::singleShot(100, [this, releaseEvent]() {
            QApplication::postEvent(this, releaseEvent);
        });
    }
}

bool DeviceWindow::event(QEvent *event)
{
    int type = 0;
    Qt::MouseButton button = Qt::NoButton;

    switch (event->type())
    {
    case QEvent::MouseButtonPress:
        type = 1;
        button = static_cast<QMouseEvent *>(event)->button();
        break;
    case QEvent::MouseButtonRelease:
        type = 2;
        button = static_cast<QMouseEvent *>(event)->button();
        break;
    case QEvent::MouseMove:
        type = 3;
        break;
    }

    if ((type == 1 || type == 2) && button == Qt::LeftButton || type == 3)
    {
        auto pos = this->getTransformedPosition(static_cast<QMouseEvent *>(event));

        QJsonObject dataObject;
        dataObject["type"] = type;
        dataObject["x"] = pos.x();
        dataObject["y"] = pos.y();

        connection->send("mouse", dataObject);
    }

    return QWidget::event(event);
}

void DeviceWindow::keyPressEvent(QKeyEvent *event)
{
    QWidget::keyPressEvent(event);

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
            QClipboard *clipboard = QGuiApplication::clipboard();
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
        Qt::Key_Backspace, Qt::Key_Delete, Qt::Key_Up, Qt::Key_Down,
        Qt::Key_Left, Qt::Key_Right, Qt::Key_Enter, Qt::Key_Return
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

    QWidget::keyReleaseEvent(event);
}

void DeviceWindow::inputMethodEvent(QInputMethodEvent *event)
{
    QString commitText = event->commitString();
    if (!commitText.isEmpty())
    {
        qDebugEx() << "输入内容:" << commitText;
        connection->send("inputText", commitText);
    }

    QWidget::inputMethodEvent(event);
}

void DeviceWindow::wheelEvent(QWheelEvent *event)
{
    qDebugEx() << "wheelEvent" << event;

    accumulatedDelta += event->angleDelta().y();

    if (wheelTimer)
        return;

    wheelTimer = new QTimer(this);

    currentPos = event->position().toPoint();
    stepCount = 0;

    auto *pressEvent = new QMouseEvent(
        QEvent::MouseButtonPress,
        currentPos,
        Qt::LeftButton,
        Qt::LeftButton,
        Qt::NoModifier
    );
    QApplication::sendEvent(this, pressEvent);

    connect(wheelTimer, &QTimer::timeout, this, [=]() mutable {
        int stepDelta;
        
        if (stepCount == maxSteps - 1)
            stepDelta = accumulatedDelta;// 最后一步，直接把剩余全部加上
        else
            stepDelta = accumulatedDelta / (maxSteps - stepCount);
        
        currentPos += QPoint(0, stepDelta);
        accumulatedDelta -= stepDelta;
        stepCount++;

        auto *moveEvent = new QMouseEvent(
            QEvent::MouseMove,
            currentPos,
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::NoModifier
        );
        QApplication::sendEvent(this, moveEvent);

        if (stepCount >= maxSteps) {
            auto *releaseEvent = new QMouseEvent(
                QEvent::MouseButtonRelease,
                currentPos,
                Qt::LeftButton,
                Qt::LeftButton,
                Qt::NoModifier
            );
            QApplication::sendEvent(this, releaseEvent);

            wheelTimer->stop();
            wheelTimer->deleteLater();
            wheelTimer = nullptr;
            accumulatedDelta = 0;
        }
    });
    wheelTimer->start(30);
}
