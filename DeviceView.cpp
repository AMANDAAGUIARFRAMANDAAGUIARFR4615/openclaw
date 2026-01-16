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
#include "DeviceWidget.h"
#include "DeviceWindow.h"
#include "Safe.h"
#include <QLayout>
#include <QLabel>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QDir>
#include <QClipboard>
#include <QVideoSink>
#include <QVideoFrame>
#include <QNetworkReply>

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

    EventHub::on(this, "clipboard", [this](const QJsonValue &data, DeviceConnection* connection) {
        if (this->connection != connection)
            return;

        if (!videoFrameWidget)
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
}

DeviceView::~DeviceView()
{
    EventHub::off(this, "clipboard");
    EventHub::off(this, "orientation");
}

void DeviceView::setSourceDevice(QIODevice *device, const QUrl &sourceUrl)
{
    addVideoFrameWidget(new VideoFrameWidget(connection, this));

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
        new ToastWidget(HIDE("设备已过期"), this);
        return;
    }

    auto isMultiControl = MainWindow::getInstance()->multiControlSwitchButton->isChecked();

    auto menu = new QMenu(this);

    auto send = [=](const QString& event, const QJsonValue &jsonValue = QJsonValue()) {
        if (isMultiControl) {
            auto devices = MainWindow::getInstance()->getDevices();

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
        QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, text);
        finder.toNextBoundary();
        int splitPos = finder.position();

        auto iconPart = text.left(splitPos);
        auto labelPart = text.mid(splitPos);

        if (labelPart == "主屏幕") {
            menu->addAction(text, [&](){send("homeScreen");});
        }
        if (labelPart == "控制中心") {
            menu->addAction(text, [&](){send("showCenterController");});
        }
        if (labelPart == "应用切换") {
            menu->addAction(text, [&](){send("appSwitcher");});
        }
        else if (labelPart == "清理应用") {
            menu->addAction(text, [&](){send("killAllApp");});
        }
        else if (labelPart == "文件管理") {
            menu->addAction(text, [this](){RemoteFileExplorer::open(connection);});
        }
        else if (labelPart == "录制+回放") {
            menu->addAction(text, [this](){Recorder::open(connection);});
        }
        else if (labelPart == "应用管理") {
            menu->addAction(text, [this](){AppListWidget::open(connection);});
        }
        else if (labelPart == "截图") {
            menu->addAction(text, [this](){connection->send("screenshot");})->setEnabled(!isMultiControl);
        }
        else if (labelPart == "重启") {
            menu->addAction(text, [&](){send("reboot");});
        }
        else if (labelPart == "锁屏") {
            if (deviceInfo->lockedStatus) {
                menu->addAction("🔓解锁", [&](){send("changeScreenLockedStatus", 0);});
            } else {
                menu->addAction("🔒锁屏", [&](){send("changeScreenLockedStatus", 1);});
            }
        }
        else if (labelPart == "清空相册") {
            menu->addAction(text, [&](){send("deleteAllPhotos");});
        }
        else if (labelPart == "音量+") {
            menu->addAction(text, [&](){send("volumeControl", "+");});
        }
        else if (labelPart == "音量-") {
            menu->addAction(text, [&](){send("volumeControl", "-");});
        }
        else if (labelPart == "同屏操作") {
            if (qobject_cast<DeviceWindow*>(this)) {
                auto enabled = MainWindow::getInstance()->multiControlSwitchButton->isChecked();
                auto action =menu->addAction(text, [=]() {
                    MainWindow::getInstance()->multiControlSwitchButton->setChecked(!enabled);
                });
                action->setCheckable(true);
                action->setChecked(enabled);
            }
        }
        else if (labelPart == "置顶") {
            if (qobject_cast<DeviceWindow*>(this)) {
                Qt::WindowFlags flags = windowFlags();

                auto action = menu->addAction(text, [=]() {
                    const auto& devices = MainWindow::getInstance()->multiControlSwitchButton->isChecked() ? MainWindow::getInstance()->getDeviceWindows() : (QList<DeviceWindow*>() << (DeviceWindow*)this);

                    for (const auto& deviceWidget : std::as_const(devices)) {
                        auto f = deviceWidget->windowFlags();
                        auto title = deviceWidget->windowTitle();

                        if (flags & Qt::WindowStaysOnTopHint)
                        {
                            f &= ~Qt::WindowStaysOnTopHint;
                            title.replace("📌", "");
                        }
                        else
                        {
                            f |= Qt::WindowStaysOnTopHint;
                            if (!title.contains("📌")) title += "📌";
                        }

                        deviceWidget->setWindowFlags(f);
                        deviceWidget->setWindowTitle(title);
                        deviceWidget->show();
                    }
                });

                action->setCheckable(true);
                action->setChecked(flags & Qt::WindowStaysOnTopHint);
            }
        }
        else if (labelPart == "修改分组") {
            menu->addAction(text, [this]() {
                if (MainWindow::getInstance()->getTabs().count() <= 1) {
                    QToolTip::showText(QCursor::pos(), "请先右键点击标签页添加自定义分组");
                    return;
                }

                BitMaskEditorDialog dialog(MainWindow::getInstance()->getTabs(), deviceInfo->groupMask, this);
                dialog.setWindowTitle("修改分组");
                if (dialog.exec() != QDialog::Accepted) return;

                settings->setValue(deviceInfo->deviceId + "/groupMask", deviceInfo->groupMask);
                MainWindow::getInstance()->relayoutDevices();
            });
        }
        else if (labelPart == "更新手机端") {
            auto dynamicSubMenu = new QMenu(text, menu);
            menu->addMenu(dynamicSubMenu);

            dynamicSubMenu->addAction("正在加载...")->setEnabled(false);

            dynamicSubMenu->setProperty("isLoaded", false);

            connect(dynamicSubMenu, &QMenu::aboutToShow, [=]() {
                if (dynamicSubMenu->property("isLoaded").toBool())
                    return;

                QString url = deviceInfo->jbType == 1 ? "https://cydia-1302990626.cos.ap-guangzhou.myqcloud.com" : "https://sileo-1302990626.cos.ap-guangzhou.myqcloud.com";
                QNetworkRequest request(url + "/Packages");
                QNetworkReply *reply = networkAccessManager->get(request);

                connect(reply, &QNetworkReply::finished, dynamicSubMenu, [=]() {
                    reply->deleteLater();

                    dynamicSubMenu->clear();

                    if (reply->error() != QNetworkReply::NoError) {
                        dynamicSubMenu->addAction(reply->errorString())->setEnabled(false);
                        return;
                    }

                    QString content = QString::fromUtf8(reply->readAll());

                    QString targetArch = QStringList({"iphoneos-arm", "iphoneos-arm64", "iphoneos-arm64e"}).value(deviceInfo->jbType - 1);

                    // Packages 文件通常由空行分隔每个包的信息
                    // 使用正则表达式分割块，兼容 \n\n 或 \r\n\r\n
                    QStringList packageBlocks = content.split(QRegularExpression("\\n\\s*\\n"), Qt::SkipEmptyParts);

                    // 遍历每一个包块
                    for (const QString &block : packageBlocks) {
                        QString pkgName;
                        QString pkgVer;
                        QString pkgArch;
                        QString pkgFile;

                        // 按行解析当前块
                        QStringList lines = block.split('\n', Qt::SkipEmptyParts);
                        for (const QString &line : lines) {
                            // 简单的字符串查找，提取 Key: Value
                            if (line.startsWith("Package: "))
                                pkgName = line.mid(9).trimmed();
                            else if (line.startsWith("Version: "))
                                pkgVer = line.mid(9).trimmed();
                            else if (line.startsWith("Architecture: "))
                                pkgArch = line.mid(14).trimmed();
                            else if (line.startsWith("Filename: "))
                                pkgFile = line.mid(10).trimmed();
                        }

                        if (pkgName != "com.sky.ykpro" || pkgArch != targetArch)
                            continue;

                        auto action = dynamicSubMenu->addAction(pkgVer);

                        connect(action, &QAction::triggered, [=](){
                            QNetworkRequest request(url + "/" + pkgFile);
                            QNetworkReply *reply = networkAccessManager->get(request);

                            connect(reply, &QNetworkReply::finished, this, [=]() {
                                reply->deleteLater();

                                if (reply->error() != QNetworkReply::NoError) {
                                    new ToastWidget(reply->errorString(), this);
                                    return;
                                }

                                const auto& data = reply->readAll();

                                // 获取系统临时文件夹路径 (例如 Windows 的 C:/Users/xxx/AppData/Local/Temp)
                                QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);

                                QString localPath = QDir(tempDir).filePath(pkgFile.section('/', -1));

                                QFile file(localPath);
                                if (!file.open(QIODevice::WriteOnly)) {
                                    new ToastWidget("无法写入临时文件: " + file.errorString(), this);
                                    return;
                                }

                                file.write(data);
                                file.close();

                                QList<QUrl> urls;
                                urls << QUrl::fromLocalFile(localPath);

                                QMimeData *mimeData = new QMimeData();
                                mimeData->setUrls(urls);

                                QDragEnterEvent dragEnterEvent(QPoint(0, 0), Qt::CopyAction, mimeData, Qt::LeftButton, Qt::NoModifier);
                                qApp->sendEvent(this, &dragEnterEvent);

                                QDropEvent dropEvent(QPoint(0, 0), Qt::CopyAction, mimeData, Qt::LeftButton, Qt::NoModifier);
                                qApp->sendEvent(this, &dropEvent);

                                delete mimeData;
                            });
                        });
                    }

                    dynamicSubMenu->setProperty("isLoaded", true);
                });
            });
        }

#ifdef Q_OS_WIN 
        auto lastAction = menu->actions().last();
        lastAction->setText(labelPart);
        lastAction->setIcon(EmojiIconProvider::createIcon(iconPart));
#endif
    }

    menu->exec(event->globalPos());
    menu->deleteLater();
}

void DeviceView::dragEnterEvent(QDragEnterEvent *event)
{
    qDebugEx() << "dragEnterEvent" << this;

    QStringList allowedSuffixes = {
        "deb", "ipa",
        "jpg", "jpeg", "png", "bmp", "gif", "webp",
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

    static bool isDispatching = false;

    if (!isDispatching && MainWindow::getInstance()->multiControlSwitchButton->isChecked()) {
        switch (event->type()) {
        // --- 键盘 ---
        case QEvent::KeyPress:
        case QEvent::KeyRelease:
            if (static_cast<QKeyEvent*>(event)->matches(QKeySequence::Paste) && MainWindow::getInstance()->lineDispatcherSwitchButton->isChecked())
                return QWidget::event(event);
        // --- 鼠标 ---
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        // case QEvent::MouseButtonDblClick:
        case QEvent::MouseMove:
        // case QEvent::Wheel:
        // --- 触摸 (Touch) ---
        // case QEvent::TouchBegin:
        // case QEvent::TouchUpdate:
        // case QEvent::TouchEnd:
        // --- 平板/手写笔 (Tablet) ---
        // case QEvent::TabletPress:
        // case QEvent::TabletRelease:
        // case QEvent::TabletMove:
        // --- 拖放 (Drag & Drop) ---
        // case QEvent::DragEnter:
        // case QEvent::DragMove:
        // case QEvent::DragLeave:
        case QEvent::Drop:
        case QEvent::Close:
            isDispatching = true;
            const auto& deviceWidgets = MainWindow::getInstance()->getDeviceWidgets();
            for (const auto& item : deviceWidgets) {
                auto targetWindow = item->getDeviceWindow();
                if (item == this || targetWindow == (DeviceWindow*)this)
                    continue;

                if (event->type() == QEvent::Drop) {
                    item->dropEvent((QDropEvent*)event);
                    continue;
                }
                
                if (event->type() == QEvent::Close) {
                    targetWindow ? targetWindow->close() : 0;
                    continue;
                }

                if (auto *mouseEvent = dynamic_cast<QMouseEvent*>(event)) {
                    DeviceView* sourceView = this;

                    if (auto widget = qobject_cast<DeviceWidget*>(this)) {
                        if (auto window = widget->getDeviceWindow())
                            sourceView = window;
                    }

                    auto targetView = item->getDeviceWindow() ? (DeviceView*)item->getDeviceWindow() : item;

                    qreal ratioX = (qreal)targetView->width() / sourceView->width();
                    qreal ratioY = (qreal)targetView->height() / sourceView->height();

                    QMouseEvent mappedEvent(mouseEvent->type(),
                                            QPointF(mouseEvent->position().x() * ratioX, mouseEvent->position().y() * ratioY),
                                            mouseEvent->globalPosition(),
                                            mouseEvent->button(),
                                            mouseEvent->buttons(),
                                            mouseEvent->modifiers(),
                                            mouseEvent->pointingDevice());
                    targetView->event(&mappedEvent);
                    continue;
                }
                
                ((DeviceView*)item)->event(event);
            }
            isDispatching = false;
            break;
        }
    }

    auto mouseEvent = dynamic_cast<QMouseEvent *>(event);
    if (!mouseEvent)
        return QWidget::event(event);

    if (deviceInfo->expireAt.get() < Account::getInstance()->loginTime.get() + elapsedTimer->elapsed()) {
        new ToastWidget(HIDE("设备已过期"), this);
        return true;
    }

    if (mouseEvent->button() == Qt::LeftButton && (mouseEvent->modifiers() & Qt::ControlModifier)) {
        event->ignore();
        return true;
    }

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

    if (event->matches(QKeySequence::SelectAll))
        event->accept();
    
    const int key = event->key();
    const auto modifiers = event->modifiers() & ~Qt::KeypadModifier;

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

            if (!MainWindow::getInstance()->lineDispatcherSwitchButton->isChecked())
            {
                connection->send("clipboard", QJsonObject{{"type", 1}, {"content", content}});
                return;
            }

            const auto& array = content.split("\n");
            const auto& devices = MainWindow::getInstance()->getDevices();
            if (array.size() != devices.size()) {
                new ToastWidget(QString("您复制的%1行文本和%2台设备不匹配").arg(array.size()).arg(devices.size()), this);
                return;
            }

            for (auto i = 0; i < array.size(); i++) {
                if (!array[i].isEmpty())
                    devices[i]->connection->send("clipboard", QJsonObject{{"type", 1}, {"content", array[i]}});
            }
            return;
        }

        if (mimeData->hasImage())
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
        qApp->postEvent(this, new QMouseEvent(QEvent::MouseButtonPress, event->position(), event->globalPosition(), event->button(), event->buttons(), event->modifiers()));
        qApp->postEvent(this, new QMouseEvent(QEvent::MouseButtonRelease, event->position(), event->globalPosition(), event->button(), event->buttons(), event->modifiers()));
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
            mapToGlobal(currentPos),
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
        mapToGlobal(currentPos),
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
            mapToGlobal(currentPos),
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::NoModifier
        );
        qApp->postEvent(this, moveEvent);

        if (stepCount >= maxSteps) {
            auto releaseEvent = new QMouseEvent(
                QEvent::MouseButtonRelease,
                currentPos,
                mapToGlobal(currentPos),
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
