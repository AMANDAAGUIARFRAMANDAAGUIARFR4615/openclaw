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

DeviceWindow::DeviceWindow(QTcpSocket* socket, DeviceInfo* deviceInfo, DeviceWidget* deviceWidget) : DeviceView(socket, deviceInfo), deviceWidget(deviceWidget)
{
    setAttribute(Qt::WA_InputMethodEnabled, true);

    videoFrameWidget = deviceWidget->getVideoFrameWidget();
    mediaSource = deviceWidget->mediaSource;

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(videoFrameWidget);

    setLayout(layout);

    EventHub::StartListening("lockedStatus", [this](const QJsonValue &data, QTcpSocket* socket) {
        if (this->socket != socket)
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

        QJsonObject jsonObject;
        jsonObject["event"] = "mouse";
        jsonObject["data"] = dataObject;
        TcpServer::sendData(socket, jsonObject);
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

    if (event->modifiers() == Qt::ControlModifier) {
        if (event->key() == Qt::Key_A || event->key() == Qt::Key_C || event->key() == Qt::Key_X)
        {
            QJsonObject dataObject;
            dataObject["type"] = "keyPress";
            dataObject["key"] = QKeySequence(event->modifiers()).toString() + keySequence;

            QJsonObject jsonObject;
            jsonObject["event"] = "keyboard";
            jsonObject["data"] = dataObject;

            TcpServer::sendData(socket, jsonObject);
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

            QJsonObject jsonObject;
            jsonObject["event"] = "clipboard";
            jsonObject["data"] = dataObject;
            TcpServer::sendData(socket, jsonObject);

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

        QJsonObject jsonObject;
        jsonObject["event"] = "keyboard";
        jsonObject["data"] = dataObject;

        TcpServer::sendData(socket, jsonObject);
        return;
    }

    auto keyText = event->text();

    if (keyText.isEmpty())
        return;

    qDebugEx() << "按键输入:" << keyText;

    QJsonObject jsonObject;
    jsonObject["event"] = "inputText";
    jsonObject["data"] = keyText;

    TcpServer::sendData(socket, jsonObject);
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

        QJsonObject jsonObject;
        jsonObject["event"] = "inputText";
        jsonObject["data"] = commitText;

        TcpServer::sendData(socket, jsonObject);
    }

    QWidget::inputMethodEvent(event);
}

void DeviceWindow::wheelEvent(QWheelEvent *event)
{
    QWidget::wheelEvent(event);

    // int delta = event->angleDelta().y();

    // // 将当前位置保存在一个非const的QPoint中
    // QPoint currentPos = event->position().toPoint();  // 拷贝构造，当前点不再是const

    // QTimer *timer = new QTimer(this);
    // connect(timer, &QTimer::timeout, [=]() mutable {  // 使用 mutable 允许修改 lambda 捕获的值
    //     QPoint newPos = currentPos + QPoint(0, delta);
    //     QMouseEvent *moveEvent = new QMouseEvent(QEvent::MouseMove, newPos, Qt::LeftButton, Qt::LeftButton, event->modifiers());
    //     QApplication::postEvent(this, moveEvent);

    //     currentPos = newPos;  // 更新当前位置

    //     // 触发一定次数后停止定时器
    //     static int count = 0;
    //     if (++count > 5) {  // 设置滑动次数
    //         timer->stop();
    //         delete timer;
    //         count = 0;
    //     }
    // });

    // // 设置定时器间隔，模拟滑动速度
    // timer->start(50);
}
