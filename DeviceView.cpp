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
#include "ScreenshotGalleryDialog.h"
#include "VideoGalleryDialog.h"
#include "Account.h"
#include "DeviceWidget.h"
#include "DeviceWindow.h"
#include "VideoFrameWidget.h"
#include <QLayout>
#include <QLabel>
#include <QPushButton>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QDir>
#include <QClipboard>
#include <QVideoSink>
#include <QVideoFrame>
#include <QNetworkReply>
#include <QResizeEvent>
#include <QFontMetrics>
#include <algorithm>

DeviceView::DeviceView(DeviceConnection* connection, DeviceInfo* deviceInfo, QWidget *parent)
    : connection(connection), deviceInfo(deviceInfo), QWidget(parent)
{
    setAcceptDrops(true);
    setAttribute(Qt::WA_InputMethodEnabled, true);

    overlay = new QWidget(this);
    overlay->setStyleSheet("background-color: black;");
    overlayLabel = new QLabel("投屏加载中...", overlay);
    overlayLabel->setAlignment(Qt::AlignCenter);
    overlayLabel->setWordWrap(false);

    overlayUnlockButton = new QPushButton("解锁", overlay);
    overlayUnlockButton->setFixedSize(110, 34);
    overlayUnlockButton->hide();
    connect(overlayUnlockButton, &QPushButton::clicked, this, [this]() {
        send("changeScreenLockedStatus", 0);
    });

    QVBoxLayout *layout = new QVBoxLayout(overlay);
    layout->addStretch();
    layout->addWidget(overlayLabel, 0, Qt::AlignHCenter);
    layout->addSpacing(8);
    layout->addWidget(overlayUnlockButton, 0, Qt::AlignHCenter);
    layout->addStretch();
    overlay->setLayout(layout);
    updateOverlayControls();

    if (!clipboardTimer) {
        clipboardTimer = new QTimer;
        clipboardTimer->setSingleShot(true);
        clipboardTimer->setInterval(3000);
        connect(clipboardTimer, &QTimer::timeout, []() {
            clipboardTotal = 0;

            QStringList lines;
            for (const auto& deviceWidget : MainWindow::getInstance()->getDeviceWidgets()) {
                if (!deviceWidget->deviceInfo->clipboardText.isEmpty())
                    lines.append(deviceWidget->deviceInfo->clipboardText);
            }

            if (lines.count() == 0)
                return;

            qApp->clipboard()->setText(lines.join('\n'));
            new ToastWidget("部分文本已复制到剪切板");
        });
    }

    EventHub::on(this, "clipboard", [this](const QJsonValue &data, DeviceConnection* connection) {
        if (this->connection != connection)
            return;

        if (!videoFrameWidget)
            return;

        auto type = data["type"].toInt();
        auto content = data["content"].toString();

        if (type == 1 || type == 3 || type == 4)
        {
            if (clipboardTotal == 0) {
                // 手机上触发的复制
                if (settings->value("autoSyncClipboard").toBool()) {
                    if (MainWindow::getInstance()->getDeviceWidgets().count() > 1) {
                        this->deviceInfo->clipboardText = content;

                        static QString clipboardText;
                        clipboardText = content;

                        static QTimer timer;
                        static bool isInitialized = false;
                        if (!isInitialized) {
                            timer.setSingleShot(true);
                            timer.setInterval(1000);
                            connect(&timer, &QTimer::timeout, []() {
                                QStringList lines;
                                for (const auto& deviceWidget : MainWindow::getInstance()->getDeviceWidgets()) {
                                    if (!deviceWidget->deviceInfo->clipboardText.isEmpty()) {
                                        lines.append(deviceWidget->deviceInfo->clipboardText);
                                        deviceWidget->deviceInfo->clipboardText = "";
                                    }
                                }

                                if (lines.count() > 0)
                                    qApp->clipboard()->setText(lines.join('\n'));
                                else
                                    qApp->clipboard()->setText(clipboardText);

                                new ToastWidget("手机剪切板内容已同步");
                            });

                            isInitialized = true;
                        }

                        timer.start();
                    }
                    else {
                        qApp->clipboard()->setText(content);
                        new ToastWidget("文本已复制到剪切板", this);
                    }
                }
                return;
            }

            if (clipboardTotal > 1) {
                if (!clipboardTimer->isActive())
                    return;

                this->deviceInfo->clipboardText = content;

                clipboardCount++;
                if (clipboardCount == clipboardTotal) {
                    clipboardTimer->stop();
                    clipboardTotal = 0;

                    QStringList lines;
                    for (const auto& deviceWidget : MainWindow::getInstance()->getDeviceWidgets()) {
                        lines.append(deviceWidget->deviceInfo->clipboardText);
                    }

                    qApp->clipboard()->setText(lines.join('\n'));
                    new ToastWidget("所有文本已复制到剪切板");
                }
            }
            else {
                clipboardTotal = 0;
                qApp->clipboard()->setText(content);
                new ToastWidget("文本已复制到剪切板", this);
            }
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

            const auto& screenshotTo = settings->value("screenshotTo").toInt();
            
            if (screenshotTo != 1) {
                qApp->clipboard()->setPixmap(QPixmap::fromImage(image));
                new ToastWidget("图片已复制到剪切板", this);
            }

            if (screenshotTo != 0) {
                const QString dirPath = Tools::screenshotSaveDirectory(this->deviceInfo->deviceId);
                QDir dir(dirPath);
                if (!dir.exists())
                    dir.mkpath(".");

                static int screenshotIndex = settings->value("screenshotIndex", 1).toInt();

                const auto& fileName = QString("%1/%2.jpg").arg(dirPath).arg(screenshotIndex);

                if (image.save(fileName))
                    settings->setValue("screenshotIndex", ++screenshotIndex);
                else
                    new ToastWidget("图片保存失败", this);
            }
            return;
        }

        new ToastWidget("此类型暂不支持", this);
    });

    connect(AppSettingsDialog::getInstance(), &AppSettingsDialog::configurationChanged, this, [this](const QString &key) {
        if (key == "windowMenu")
        {
            addContextMenuActions();
            return;
        }
    });
}

DeviceView::~DeviceView()
{
    EventHub::off(this, "clipboard");
}

void DeviceView::setSourceDevice(QIODevice *device, const QUrl &sourceUrl)
{
    if (!videoFrameWidget)
        addVideoFrameWidget(new VideoFrameWidget(connection, this));

    auto mediaPlayer = videoFrameWidget->mediaPlayer;
    mediaPlayer->setSourceDevice(device);
    mediaPlayer->play();
}

void DeviceView::showOverlay(const QString &text)
{
    overlayLabel->setText(text);
    overlayUnlockButton->setVisible(text == "设备已锁定");
    updateOverlayControls();

    overlay->show();
    overlay->raise();

    QTimer::singleShot(0, [this]() {
        if (videoFrameWidget)
            videoFrameWidget->hide();
    });
}

void DeviceView::hideOverlay()
{
    overlayUnlockButton->hide();
    overlay->hide();

    if (videoFrameWidget)
        videoFrameWidget->show();
}

void DeviceView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateOverlayControls();
}

void DeviceView::updateOverlayControls()
{
    const int viewW = std::max(1, width());
    const int viewH = std::max(1, height());
    const int minSide = std::min(viewW, viewH);
    const int horizontalPadding = std::clamp(viewW / 20, 4, 28);
    const int verticalPadding = std::clamp(viewH / 20, 4, 28);
    const int horizontalSpace = std::max(1, viewW - horizontalPadding * 2);

    overlay->setGeometry(0, 0, viewW, viewH);

    const QColor windowColor = qApp->palette().color(QPalette::Window);
    const bool isLightScheme = windowColor.lightness() > 128;
    const QString labelColor = isLightScheme ? QStringLiteral("#F8FAFC") : QStringLiteral("#E5E7EB");
    const QString buttonBg = isLightScheme ? QStringLiteral("rgba(255, 255, 255, 235)") : QStringLiteral("rgba(30, 41, 59, 230)");
    const QString buttonBorder = isLightScheme ? QStringLiteral("rgba(148, 163, 184, 210)") : QStringLiteral("rgba(148, 163, 184, 220)");
    const QString buttonText = isLightScheme ? QStringLiteral("#0F172A") : QStringLiteral("#F8FAFC");
    const QString buttonHoverBg = isLightScheme ? QStringLiteral("rgba(255, 255, 255, 255)") : QStringLiteral("rgba(51, 65, 85, 235)");
    const QString buttonPressedBg = isLightScheme ? QStringLiteral("rgba(241, 245, 249, 255)") : QStringLiteral("rgba(30, 41, 59, 255)");
    overlayLabel->setStyleSheet(QStringLiteral("color: %1;").arg(labelColor));
    overlayUnlockButton->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "background-color: %1;"
        "border: 1px solid %2;"
        "color: %3;"
        "}"
        "QPushButton:hover { background-color: %4; }"
        "QPushButton:pressed { background-color: %5; }")
        .arg(buttonBg, buttonBorder, buttonText, buttonHoverBg, buttonPressedBg));

    if (auto *layout = qobject_cast<QVBoxLayout *>(overlay->layout())) {
        layout->setContentsMargins(horizontalPadding, verticalPadding, horizontalPadding, verticalPadding);
        layout->setSpacing(std::clamp(minSide / 40, 2, 10));
    }

    // Keep text readable on small views while avoiding oversized fonts on large views.
    const int labelPixelSize = std::clamp(minSide / 9, 10, 26);
    QFont labelFont = overlayLabel->font();
    labelFont.setPixelSize(labelPixelSize);
    overlayLabel->setFont(labelFont);
    overlayLabel->setMaximumWidth(horizontalSpace);
    const QFontMetrics labelMetrics(labelFont);

    QString overlayText = overlayLabel->text();
    if (overlayText == "设备已锁定" || overlayText == "已锁定" || overlayText == "锁定") {
        if (labelMetrics.horizontalAdvance("设备已锁定") <= horizontalSpace)
            overlayText = "设备已锁定";
        else if (labelMetrics.horizontalAdvance("已锁定") <= horizontalSpace)
            overlayText = "已锁定";
        else
            overlayText = "锁定";
        overlayLabel->setText(overlayText);
    }

    QFont buttonFont = overlayUnlockButton->font();
    buttonFont.setPixelSize(std::clamp(labelPixelSize - 2, 9, 20));
    overlayUnlockButton->setFont(buttonFont);

    const QFontMetrics buttonMetrics(buttonFont);
    const int minButtonW = buttonMetrics.horizontalAdvance(overlayUnlockButton->text()) + std::clamp(viewW / 10, 16, 28);
    const int maxButtonW = std::max(minButtonW, std::min(horizontalSpace, 180));
    const int buttonW = std::clamp(viewW * 34 / 100, minButtonW, maxButtonW);

    const int minButtonH = buttonMetrics.height() + std::clamp(viewH / 22, 8, 14);
    const int maxButtonH = std::max(minButtonH, 44);
    const int buttonH = std::clamp(viewH * 10 / 100, minButtonH, maxButtonH);
    overlayUnlockButton->setFixedSize(buttonW, buttonH);
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

void DeviceView::send(const DataGuard::StrObfuscator<>& event, const QJsonValue &jsonValue) {
    const auto& connections = MainWindow::getInstance()->getDeviceConnections(this);
    for (const auto& connection : connections) {
        connection->send(event, jsonValue);
    }
}

void DeviceView::addContextMenuActions()
{
    qDeleteAll(actions());

    QLayout* buttonLayout = findChild<QLayout*>("buttonLayout");
    
    if (buttonLayout) {
        QLayoutItem *child;
        while (child = buttonLayout->takeAt(0)) {
            if (child->widget())
                delete child->widget();
            
            delete child;
        }
    }

    auto windowMenu = AppSettingsDialog::getInstance()->getEnabledList("windowMenu");

    for (int i = 0; i < windowMenu.count(); i++) {
        auto text = windowMenu[i];
        QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, text);
        finder.toNextBoundary();
        int splitPos = finder.position();

        auto labelPart = text.mid(splitPos);

        if (labelPart == "主屏幕") {
            addAction(text, [=](){send("homeScreen");});
        }
        if (labelPart == "控制中心") {
            addAction(text, [=](){send("showCenterController");});
        }
        if (labelPart == "应用切换") {
            addAction(text, [=](){send("appSwitcher");});
        }
        else if (labelPart == "清理应用") {
            addAction(text, [=](){send("killAllApp");});
        }
        else if (labelPart == "文件管理") {
            addAction(text, [this](){RemoteFileExplorer::open(connection, "/", this);});
        }
        else if (labelPart == "录制+回放") {
            addAction(text, [this](){Recorder::open(connection, this);});
        }
        else if (labelPart == "应用管理") {
            addAction(text, [this](){AppListWidget::open(connection, this);});
        }
        else if (labelPart == "截图") {
            addAction(text, [this](){connection->send("screenshot");});
        }
        else if (labelPart == "截图库") {
            addAction(text, [this]() { ScreenshotGalleryDialog::open(this, deviceInfo->deviceId); });
        }
        else if (labelPart == "投屏录像") {
            DeviceWidget *dw = qobject_cast<DeviceWidget *>(this);
            if (!dw)
                if (auto *w = qobject_cast<DeviceWindow *>(this))
                    dw = w->deviceWidget;
            if (!dw)
                continue;
            auto *action = addAction(text, [dw]() { dw->setStreamRecording(!dw->isStreamRecording()); });
            action->setCheckable(true);
            action->setChecked(dw->isStreamRecording());
        }
        else if (labelPart == "录像库") {
            addAction(text, [this]() { VideoGalleryDialog::open(this, deviceInfo->deviceId); });
        }
        else if (labelPart == "重启") {
            addAction(text, [=](){send("reboot");});
        }
        else if (labelPart == "锁屏") {
            addAction(text, [=](){send("changeScreenLockedStatus", deviceInfo->lockedStatus ? 0 : 1);});
        }
        else if (labelPart == "获取剪切板") {
            addAction(text, [=](){
                const auto& deviceWidgets = MainWindow::getInstance()->getDeviceWidgets(this);
                for (const auto& deviceWidget : deviceWidgets) {
                    deviceWidget->deviceInfo->clipboardText = "";
                }

                clipboardTotal = deviceWidgets.count();
                clipboardCount = 0;

                if (clipboardTotal > 1)
                    clipboardTimer->start();

                send("getClipboard");
            });
        }
        else if (labelPart == "清空相册") {
            addAction(text, [=](){send("deleteAllPhotos");});
        }
        else if (labelPart == "音量+") {
            addAction(text, [=](){send("volumeControl", "+");});
        }
        else if (labelPart == "音量-") {
            addAction(text, [=](){send("volumeControl", "-");});
        }
        else if (labelPart == "主控") {
            auto action = addAction(text, [this]() {
                deviceInfo->controller = !deviceInfo->controller;
                settings->setValue(deviceInfo->deviceId + "/controller", deviceInfo->controller);

                auto deviceWidget = qobject_cast<DeviceWidget*>(this);
                if (!deviceWidget)
                    deviceWidget = qobject_cast<DeviceWindow*>(this)->deviceWidget;
                setWindowTitle(QString("%1%2%3").arg(deviceInfo->deviceName, deviceInfo->controller ? " 🏹主控" : "", deviceWidget->checkBox->isChecked() ? " 🎯被控" : ""));
            });
            action->setCheckable(true);
            action->setChecked(deviceInfo->controller);
        }
        else if (labelPart == "被控") {
            auto deviceWidget = qobject_cast<DeviceWidget*>(this);
            if (!deviceWidget)
                deviceWidget = qobject_cast<DeviceWindow*>(this)->deviceWidget;

            auto action = addAction(text, [=]() {
                deviceWidget->checkBox->setChecked(!deviceWidget->checkBox->isChecked());

                setWindowTitle(QString("%1%2%3").arg(deviceInfo->deviceName, deviceInfo->controller ? " 🏹主控" : "", deviceWidget->checkBox->isChecked() ? " 🎯被控" : ""));
            });
            action->setCheckable(true);
            action->setChecked(deviceWidget->checkBox->isChecked());
        }
        else if (labelPart == "置顶") {
            auto action = addAction(text, [this]() {
                auto flags = windowFlags();
                
                for (const auto& deviceWindow : MainWindow::getInstance()->getDeviceWindows(this)) {
                    auto f = deviceWindow->windowFlags();
                    auto title = deviceWindow->windowTitle();

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

                    deviceWindow->setWindowFlags(f);
                    deviceWindow->setWindowTitle(title);
                    deviceWindow->show();
                }
            });

            action->setCheckable(true);
            action->setChecked(windowFlags() & Qt::WindowStaysOnTopHint);
        }
        else if (labelPart == "修改分组") {
            addAction(text, [this]() {
                if (MainWindow::getInstance()->getTabs().count() <= 1) {
                    Tools::showToast(QStringLiteral("请先右键点击标签页添加自定义分组"), this);
                    return;
                }

                QList<quint32*> maskRefs;
                auto deviceWidgets = MainWindow::getInstance()->getDeviceWidgets(this);
                
                for (const auto& deviceWidget : deviceWidgets) {
                    maskRefs.append(&(deviceWidget->deviceInfo->groupMask));
                }

                BitMaskEditorDialog dialog(MainWindow::getInstance()->getTabs(), maskRefs, this);
                dialog.setWindowTitle("修改分组");
                if (dialog.exec() != QDialog::Accepted) return;

                for (const auto& deviceWidget : deviceWidgets) {
                    settings->setValue(deviceWidget->deviceInfo->deviceId + "/groupMask", deviceWidget->deviceInfo->groupMask);
                }

                MainWindow::getInstance()->relayoutDevices();
            });
        }
        else if (labelPart == "更新手机端") {
            auto action = addAction(text);
            auto dynamicSubMenu = new QMenu();
            action->setMenu(dynamicSubMenu);

            dynamicSubMenu->addAction("正在加载...")->setEnabled(false);

            dynamicSubMenu->setProperty("isLoaded", false);

            connect(dynamicSubMenu, &QMenu::aboutToShow, [=]() {
                if (dynamicSubMenu->property("isLoaded").toBool())
                    return;

                QNetworkRequest request(Config::SITE_URL + "Packages");
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
                    std::reverse(packageBlocks.begin(), packageBlocks.end());

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

                        connect(action, &QAction::triggered, [=]() {
                            QNetworkRequest request(Config::SITE_URL + pkgFile);
                            QNetworkReply *reply = networkAccessManager->get(request);

                            new ToastWidget("正在下载中，请稍等");

                            connect(reply, &QNetworkReply::finished, this, [=]() {
                                reply->deleteLater();

                                if (reply->error() != QNetworkReply::NoError) {
                                    new ToastWidget(reply->errorString(), this);
                                    return;
                                }

                                const auto& data = reply->readAll();

                                QString localPath = QDir::temp().filePath(pkgFile.section('/', -1));

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
        else if (labelPart == "开启独占") {
            addAction(text, [this](){
                const auto& udids = MainWindow::getInstance()->getDeviceUdids(this);
                webSocketClient->emitEvent("setDeviceLocker", QJsonObject{{"udids", QJsonArray::fromStringList(udids)}, {"locked", !deviceInfo->hasLocker()}});
            });
        }
    }

    const auto& shortcutMap = AppSettingsDialog::getInstance()->getShortcuts("windowMenu");
    for(const auto& action : actions()) {
        auto text = action->text();
        action->setShortcut(shortcutMap[text]);
        action->setShortcutContext(Qt::WidgetShortcut);

        if (text == "🔒锁屏" && deviceInfo->lockedStatus)
            text = "🔓解锁";

        if (text == "🚩开启独占" && deviceInfo->hasLocker())
            text = "🏳️退出独占";

        QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, text);
        finder.toNextBoundary();
        int splitPos = finder.position();

        auto iconPart = text.left(splitPos);
        auto labelPart = text.mid(splitPos);
        action->setText(labelPart);
        action->setIcon(EmojiIconProvider::createIcon(iconPart));
        action->setIconVisibleInMenu(true);
        action->setShortcutVisibleInContextMenu(true);

        if (!buttonLayout)
            continue;

        if (action->isSeparator())
            continue;

        if (action->text().contains("更新手机端") || action->text().contains("独占"))
            continue;

        auto button = new QPushButton(action->text());

        if (!action->icon().isNull())
            button->setIcon(action->icon());
        
        button->setFixedHeight(34);

        connect(button, &QPushButton::clicked, action, &QAction::trigger);

        buttonLayout->addWidget(button);
    }
}

void DeviceView::contextMenuEvent(QContextMenuEvent *event)
{
    addContextMenuActions();

    if (actions().count() == 0)
        return;

    if (deviceInfo->expireAt.get() < Account::getInstance()->loginTime.get() + elapsedTimer->elapsed()) {
        new ToastWidget(HIDE_STR("设备已过期"), this);
        return;
    }

    auto menu = new QMenu;
    menu->addActions(actions());
    menu->exec(event->globalPos());
    menu->deleteLater();
}

void DeviceView::dragEnterEvent(QDragEnterEvent *event)
{
    qDebugEx() << "dragEnterEvent" << this;

    QStringList allowedSuffixes = {
        "deb", "ipa", "tipa",
        "jpg", "jpeg", "png", "bmp", "gif", "webp",
        "mp4", "mov", "avi", "mkv", "flv", "wmv"
    };

    for (const QUrl &url : event->mimeData()->urls())
    {
        QString suffix = QFileInfo(url.toLocalFile()).suffix().toLower();
        if (!allowedSuffixes.contains(suffix)) {
            event->setDropAction(Qt::IgnoreAction);
            event->accept();
            return;
        }
    }

    event->acceptProposedAction();
}

void DeviceView::dropEvent(QDropEvent *event)
{
    qDebugEx() << "dropEvent" << this;

    for (const QUrl& url : event->mimeData()->urls()) {
        auto localPath = url.toLocalFile();
        new FileTransfer(connection, 2, localPath, localPath.section('/', -1), this);
    }

    event->acceptProposedAction();
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
            auto mouseEvent = dynamic_cast<QMouseEvent*>(event);

            if (mouseEvent) {
                if (mouseEvent->type() != QEvent::MouseMove && mouseEvent->button() != Qt::MouseButton::LeftButton)
                    break;

                if (mouseEvent->type() == QEvent::MouseMove && !(pressedButtons & Qt::LeftButton))
                    break;

                if (mouseEvent->pointingDevice()->name() == "VirtualMouse")
                    break;
            }

            isDispatching = true;
            for (const auto& deviceWidget : MainWindow::getInstance()->getDeviceWidgets(this)) {
                auto targetWindow = deviceWidget->getDeviceWindow();
                if (deviceWidget == this || targetWindow == (DeviceWindow*)this)
                    continue;

                if (event->type() == QEvent::Drop) {
                    deviceWidget->dropEvent((QDropEvent*)event);
                    continue;
                }
                
                if (event->type() == QEvent::Close) {
                    targetWindow ? targetWindow->close() : 0;
                    continue;
                }

                if (mouseEvent) {
                    DeviceView* sourceView = this;

                    if (auto widget = qobject_cast<DeviceWidget*>(this)) {
                        if (auto window = widget->getDeviceWindow())
                            sourceView = window;
                    }

                    auto targetView = deviceWidget->getDeviceWindow() ? (DeviceView*)deviceWidget->getDeviceWindow() : deviceWidget;

                    qreal ratioX = (qreal)targetView->width() / sourceView->width();
                    qreal ratioY = (qreal)targetView->height() / sourceView->height();

                    static QPointingDevice pointingDevice("VirtualMouse", 1, QInputDevice::DeviceType::Mouse, QPointingDevice::PointerType::Generic, QInputDevice::Capability::Position, 1, 3);
                    auto mappedEvent = new QMouseEvent(mouseEvent->type(),
                                                       QPointF(mouseEvent->position().x() * ratioX, mouseEvent->position().y() * ratioY),
                                                       mouseEvent->globalPosition(),
                                                       mouseEvent->button(),
                                                       mouseEvent->buttons(),
                                                       mouseEvent->modifiers(),
                                                       &pointingDevice);

                    struct DelayedEvent {
                        qint64 execTime;         // 计划执行的时间戳
                        QMouseEvent* event;      // 事件指针
                        QPointer<DeviceView> target;// 目标对象（防崩溃保护）
                    };

                    static QQueue<DelayedEvent> eventQueue;
                    static QTimer* queueTimer = nullptr;

                    if (!queueTimer) {
                        queueTimer = new QTimer(qApp);
                        queueTimer->setInterval(0);

                        connect(queueTimer, &QTimer::timeout, []() {
                            qint64 now = QDateTime::currentMSecsSinceEpoch();

                            // 循环取出所有时间 <= 当前时间的事件
                            while (!eventQueue.isEmpty() && eventQueue.head().execTime <= now) {
                                const auto& item = eventQueue.dequeue();

                                if (item.target)
                                    item.target->event(item.event);

                                delete item.event;
                            }
                        });

                        queueTimer->start();
                    }

                    if (MainWindow::getInstance()->multiControlSwitchButton->isChecked() && MainWindow::getInstance()->randomDelayCheckBox->isChecked()) {
                        if (event->type() == QEvent::MouseButtonPress)
                            targetView->deviceInfo->randomDelay = MainWindow::getInstance()->getRandomDelay();

                        qint64 execTime = QDateTime::currentMSecsSinceEpoch() + targetView->deviceInfo->randomDelay;
                        eventQueue.enqueue({execTime, mappedEvent, targetView});
                    }
                    else {
                        targetView->event(mappedEvent);
                        delete mappedEvent;
                    }

                    continue;
                }
                
                ((DeviceView*)deviceWidget)->event(event);
            }
            isDispatching = false;
            break;
        }
    }

    auto mouseEvent = dynamic_cast<QMouseEvent *>(event);
    if (!mouseEvent)
        return QWidget::event(event);

    if (deviceInfo->expireAt.get() < Account::getInstance()->loginTime.get() + elapsedTimer->elapsed()) {
        new ToastWidget(HIDE_STR("设备已过期"), this);
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
        if (videoFrameWidget->rect().contains(localPos)) {
            auto pos = getTransformedPosition(localPos);

            QJsonObject dataObject;
            dataObject["type"] = type;
            dataObject["x"] = pos.x();
            dataObject["y"] = pos.y();

            connection->send("mouse", dataObject);
        }
    }

    if (type == 2)
        pressedButtons &= ~mouseEvent->button();

    return QWidget::event(event);
}

void DeviceView::keyPressEvent(QKeyEvent *event)
{
    QWidget::keyPressEvent(event);

    if (event->matches(QKeySequence::SelectAll))
        event->accept();
    
    const int key = event->key();
    const auto modifiers = event->modifiers() & ~Qt::KeypadModifier;

    if (modifiers == Qt::NoModifier || modifiers == Qt::ShiftModifier) {
#ifdef Q_OS_WIN
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

    if (event->matches(QKeySequence::Copy)) {
        if (hasFocus()) {
            const auto& deviceWidgets = MainWindow::getInstance()->getDeviceWidgets(this);
            for (const auto& deviceWidget : deviceWidgets) {
                deviceWidget->deviceInfo->clipboardText = "";
            }

            clipboardTotal = deviceWidgets.count();
            clipboardCount = 0;

            if (clipboardTotal > 1)
                clipboardTimer->start();
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
            const auto& connections = MainWindow::getInstance()->getDeviceConnections(this);
            if (array.size() != connections.size()) {
                new ToastWidget(QString("您复制的%1行文本和%2台设备不匹配").arg(array.size()).arg(connections.size()), this);
                return;
            }

            for (auto i = 0; i < array.size(); i++) {
                if (!array[i].isEmpty())
                    connections[i]->send("clipboard", QJsonObject{{"type", 1}, {"content", array[i]}});
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

void DeviceView::inputMethodEvent(QInputMethodEvent *event)
{
    QString commitText = event->commitString();
    if (!commitText.isEmpty())
    {
        qDebugEx() << "输入内容:" << commitText;
        send("inputText", commitText);
    }

    QWidget::inputMethodEvent(event);
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
