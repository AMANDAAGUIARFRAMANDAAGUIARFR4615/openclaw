#include "DeviceView.h"
#include "Logger.h"
#include "Tools.h"
#include "RemoteFileExplorer.h"
#include "FileTransfer.h"
#include "EventHub.h"
#include "Recorder.h"
#include "AppListWidget.h"
#include "BitMaskEditorDialog.h"
#include "MainWindow.h"
#include "EmojiIconProvider.h"
#include "KeyMapping.h"
#include "AppSettingsDialog.h"
#include "Account.h"
#include <QLayout>
#include <QLabel>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QDir>
#include <QClipboard>

DeviceView::DeviceView(DeviceConnection* connection, DeviceInfo* deviceInfo, QWidget *parent)
    : connection(connection), deviceInfo(deviceInfo), QWidget(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setAcceptDrops(true);

    overlay = new QWidget(this);
    overlay->setStyleSheet("background-color: black;");
    QLabel *label = new QLabel("----------------", overlay);
    label->setStyleSheet("color: white; font-size: 20px;");
    label->setAlignment(Qt::AlignCenter);

    QVBoxLayout *layout = new QVBoxLayout(overlay);
    layout->addStretch();
    layout->addWidget(label);
    layout->addStretch();
    overlay->setLayout(layout);
}

DeviceView::~DeviceView()
{
    EventHub::off(this, "orientation");
}

void DeviceView::setSourceDevice(QIODevice *device, const QUrl &sourceUrl)
{
    addVideoFrameWidget(new VideoFrameWidget(this));

    auto mediaPlayer = videoFrameWidget->mediaPlayer;
    mediaPlayer->setSourceDevice(device);
    mediaPlayer->play();
}

void DeviceView::showOverlay(const QString &text)
{
    QLabel *label = overlay->findChild<QLabel *>();
    label->setText(text);

    overlay->show();
    overlay->raise();

    QTimer::singleShot(0, [=]() {
        if (videoFrameWidget)
            videoFrameWidget->hide();
    });
}

void DeviceView::hideOverlay()
{
    overlay->hide();

    if (videoFrameWidget)
        videoFrameWidget->show();
}

void DeviceView::addVideoFrameWidget(VideoFrameWidget* widget)
{
    videoFrameWidget = widget;

    QBoxLayout* boxLayout = qobject_cast<QBoxLayout*>(layout());
    if (qobject_cast<QHBoxLayout*>(boxLayout))
        boxLayout->insertWidget(0, widget);
    else
        boxLayout->insertWidget(1, widget);

    if (deviceInfo->lockedStatus)
        showOverlay("设备已锁定");
    else
        hideOverlay();
}

void DeviceView::contextMenuEvent(QContextMenuEvent *event)
{
    auto windowMenu = AppSettingsDialog::getInstance()->getEnabledList("windowMenu");
    if (windowMenu.count() == 0)
        return;

    if (deviceInfo->expireAt.get() < Account::getInstance()->loginTime.get() + elapsedTimer->elapsed()) {
        new ToastWidget("设备已过期", this);
        return;
    }

    auto isMultiControl = MainWindow::getInstance()->isMultiControl();

    auto menu = new QMenu(this);
    menu->setStyleSheet(R"(
        QMenu::item {
            padding: 5px;
        }
        QMenu::item:selected {
            background-color: #E0E0E0;
        }
    )");

    auto send = [=](const QString& event, const QJsonValue &jsonValue = QJsonValue()) {
        if (isMultiControl) {
            auto bit = MainWindow::getInstance()->getTab().bit;
            auto devices = DeviceInfo::getDevices(bit == 0 ? 0 : (1U << bit));

            for (const auto& device : devices) {
                device->connection->send(event, jsonValue);
            }
        }
        else {
            connection->send(event, jsonValue);
        }
    };

    for (int i = 0; i < windowMenu.count(); i++) {
        auto text = windowMenu[i];

        if (text == "🏠主屏幕") {
            menu->addAction(text, [&](){send("homeScreen");});
        }
        if (text == "🎛️控制中心") {
            menu->addAction(text, [&](){send("showCenterController");});
        }
        if (text == "↕️应用切换") {
            menu->addAction(text, [&](){send("appSwitcher");});
        }
        else if (text == "🧹清理应用") {
            menu->addAction(text, [&](){send("killAllApp");});
        }
        else if (text == "📁文件管理") {
            menu->addAction(text, [this](){RemoteFileExplorer::open(connection);});
        }
        else if (text == "⏺️录制+回放") {
            menu->addAction(text, [this](){Recorder::open(connection);});
        }
        else if (text == "🧩应用列表") {
            menu->addAction(text, [this](){AppListWidget::open(connection);});
        }
        else if (text == "📸截图") {
            menu->addAction(text, [this](){connection->send("screenshot");})->setEnabled(!isMultiControl);
        }
        else if (text == "🔄重启") {
            menu->addAction(text, [&](){send("reboot");});
        }
        else if (text == "🔒锁屏") {
            if (deviceInfo->lockedStatus) {
                menu->addAction("🔓解锁", [&](){send("changeScreenLockedStatus", 0);});
            } else {
                menu->addAction("🔒锁屏", [&](){send("changeScreenLockedStatus", 1);});
            }
        }
        else if (text == "🗑️清空相册") {
            menu->addAction(text, [&](){send("deleteAllPhotos");});
        }
        else if (text == "🔧修改分组") {
            menu->addAction(text, [this]() {
                if (MainWindow::getInstance()->getTabs().count() <= 1) {
                    QToolTip::showText(QCursor::pos(), "请先右键点击标签页添加自定义分组");
                    return;
                }

                BitMaskEditorDialog dialog(MainWindow::getInstance()->getTabs(), deviceInfo->groupMask, this);
                dialog.setWindowTitle("修改分组");
                if (dialog.exec() != QDialog::Accepted) return;

                settings->setValue(deviceInfo->deviceId + "/groupMask", deviceInfo->groupMask);
            });
        }
    }

    menu->exec(event->globalPos());
    menu->deleteLater();
}

void DeviceView::dragEnterEvent(QDragEnterEvent *event)
{
    qDebugEx() << "dragEnterEvent" << this;

    QStringList allowedSuffixes = {
        "deb", "ipa",
        "png", "jpg", "jpeg", "bmp", "gif",
        "mp4", "mov", "avi", "mkv", "flv", "wmv"
    };

    const QList<QUrl> urls = event->mimeData()->urls();

    for (const QUrl &url : urls)
    {
        QString suffix = QFileInfo(url.toLocalFile()).suffix().toLower();
        if (!allowedSuffixes.contains(suffix))
        {
            event->ignore();
            return;
        }
    }

    event->accept();
}

void DeviceView::dropEvent(QDropEvent *event)
{
    qDebugEx() << "dropEvent" << this;

    const QList<QUrl> urls = event->mimeData()->urls();

    for (const QUrl& url : urls) {
        auto type = 2; // 收是1，发是2
        auto localPath = url.toLocalFile();
        auto size = Tools::getFileSize(localPath);
        
        auto transfer = new FileTransfer(connection, type, localPath, size);

        QJsonObject dataObject;
        dataObject["id"] = transfer->id;
        dataObject["type"] = type;
        dataObject["port"] = transfer->serverPort();
        dataObject["name"] = localPath.section('/', -1);
        dataObject["size"] = size;

        connection->send("transferFile", dataObject);
    }

    event->accept();
}

QPoint DeviceView::getTransformedPosition(QPoint pos) {
    // 1. 获取设备原始宽高
    int dw = deviceInfo->screenWidth;
    int dh = deviceInfo->screenHeight;
    
    // 2. 确定视觉上的宽高（若横屏则宽高互换）
    bool isVert = (deviceInfo->orientation == 1 || deviceInfo->orientation == 2);
    int vw = isVert ? dw : dh;
    int vh = isVert ? dh : dw;

    // 3. 计算实际缩放比和黑边偏移
    float ww = videoFrameWidget->width();
    float wh = videoFrameWidget->height();
    float scale = qMin(ww / vw, wh / vh);
    float offX = (ww - vw * scale) / 2;
    float offY = (wh - vh * scale) / 2;

    // 4. 将鼠标坐标映射到视频画面内 (去除黑边并缩放)
    int x = qBound(0, (int)((pos.x() - offX) / scale), vw);
    int y = qBound(0, (int)((pos.y() - offY) / scale), vh);

    // 5. 坐标旋转映射
    switch (deviceInfo->orientation) {
        case 2: // UpsideDown
            return QPoint(dw - x, dh - y);
        case 3: // LandscapeRight (视觉Y对应设备X(反向), 视觉X对应设备Y)
            return QPoint(dw - y, x);
        case 4: // LandscapeLeft (视觉Y对应设备X, 视觉X对应设备Y(反向))
            return QPoint(y, dh - x);
        default: // Portrait
            return QPoint(x, y);
    }
}

bool DeviceView::event(QEvent *event)
{
    if (!videoFrameWidget)
        return QWidget::event(event);

    auto mouseEvent = dynamic_cast<QMouseEvent *>(event);
    if (!mouseEvent)
        return QWidget::event(event);

    if (deviceInfo->expireAt.get() < Account::getInstance()->loginTime.get() + elapsedTimer->elapsed()) {
        new ToastWidget("设备已过期", this);
        return true;
    }

    int type = 0;

    switch (event->type())
    {
    case QEvent::MouseButtonPress:
        type = 1;
        setFocus();
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

    return QWidget::event(event);
}

void DeviceView::keyPressEvent(QKeyEvent *event)
{
    QWidget::keyPressEvent(event);

    if (event->key() == Qt::Key_Escape)
    {
        close();
        return;
    }

    const int key = event->key();
    const auto modifiers = event->modifiers();

    if (modifiers == Qt::KeypadModifier) {
        connection->send("keyboard", QJsonObject{{"type", "keyPress"}, {"key", event->text()}, {"repeat", event->isAutoRepeat()}});
        return;
    }

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

void DeviceView::keyReleaseEvent(QKeyEvent *event)
{
    qDebugEx() << "放开" << QKeySequence(event->key()).toString();

    QWidget::keyReleaseEvent(event);
}

void DeviceView::mouseDoubleClickEvent(QMouseEvent *event)
{
    QWidget::mouseDoubleClickEvent(event);

    qDebugEx() << "双击" << event->button();

    if (event->button() == Qt::LeftButton) {
        qApp->postEvent(this, new QMouseEvent(QEvent::MouseButtonPress, event->pos(), event->button(), event->button(), event->modifiers()));
        qApp->postEvent(this, new QMouseEvent(QEvent::MouseButtonRelease, event->pos(), event->button(), event->button(), event->modifiers()));
    }
}

void DeviceView::wheelEvent(QWheelEvent *event)
{
    // if (QOperatingSystemVersion::current().type() == QOperatingSystemVersion::MacOS)
    //     return;

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
        qApp->postEvent(this, moveEvent);
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
    qApp->postEvent(this, pressEvent);

    wheelTimer->callOnTimeout([=]() mutable {
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
        qApp->postEvent(this, moveEvent);

        if (stepCount >= maxSteps) {
            auto releaseEvent = new QMouseEvent(
                QEvent::MouseButtonRelease,
                currentPos,
                Qt::LeftButton,
                Qt::LeftButton,
                Qt::NoModifier
            );
            qApp->postEvent(this, releaseEvent);

            wheelTimer->stop();
            wheelTimer->deleteLater();
            wheelTimer = nullptr;
            accumulatedDelta = 0;
        }
    });

    wheelTimer->start(30);
}
