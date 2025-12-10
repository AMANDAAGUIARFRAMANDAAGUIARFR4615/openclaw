#include "DeviceView.h"
#include "Logger.h"
#include "DeviceWindow.h"
#include "Tools.h"
#include "RemoteFileExplorer.h"
#include "FileTransfer.h"
#include "EventHub.h"
#include "Recorder.h"
#include "AppListWidget.h"
#include "BitMaskEditorDialog.h"
#include "MainWindow.h"
#include "EmojiIconProvider.h"
#include <QLayout>
#include <QLabel>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QDir>

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

    EventHub::on(this, "orientation", [this](const QJsonValue &data, DeviceConnection* connection) {
        if (this->connection != connection)
            return;

        this->deviceInfo->orientation = data.toInt();

        if (videoFrameWidget)
            videoFrameWidget->orientationChanged(data.toInt());
    });
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

void DeviceView::addOverlay(const QString &text)
{
    QLabel *label = overlay->findChild<QLabel *>();
    label->setText(text);

    overlay->show();
    overlay->raise();
}

void DeviceView::addVideoFrameWidget(VideoFrameWidget* widget)
{
    videoFrameWidget = widget;
    
    QBoxLayout* boxLayout = qobject_cast<QBoxLayout*>(layout());
    if (qobject_cast<QHBoxLayout*>(boxLayout))
        boxLayout->insertWidget(0, widget);
    else
        boxLayout->insertWidget(1, widget);
    
    videoFrameWidget->orientationChanged(deviceInfo->orientation);

    if (deviceInfo->lockedStatus)
        addOverlay("设备已锁定");
    else
        overlay->hide();
}

void DeviceView::onHomeScreenClicked()
{
    connection->send("homeScreen");
}

void DeviceView::onCenterControllerClicked()
{
    connection->send("showCenterController");
}

void DeviceView::onKillAllAppClicked()
{
    connection->send("killAllApp");
}

void DeviceView::onAppSwitcherClicked()
{
    connection->send("appSwitcher");
}

void DeviceView::onFileClicked()
{
    RemoteFileExplorer::open(connection);
}

void DeviceView::onRecorderClicked()
{
    Recorder::open(connection);
}

void DeviceView::onAppListClicked()
{
    AppListWidget::open(connection);
}

void DeviceView::onScreenshotClicked()
{
    connection->send("screenshot");
}

void DeviceView::onRebootClicked()
{
    connection->send("reboot");
}

void DeviceView::onDeleteAllPhotosClicked()
{
    connection->send("deleteAllPhotos");
}

void DeviceView::onLockClicked()
{
    connection->send("changeScreenLockedStatus", 1);
}

void DeviceView::onUnlockClicked()
{
    connection->send("changeScreenLockedStatus", 0);
}

void DeviceView::onVolumeUpClicked()
{
    connection->send("volumeControl", "+");
}

void DeviceView::onVolumeDownClicked()
{
    connection->send("volumeControl", "-");
}

QMenu* DeviceView::createContextMenu()
{
    auto menu = new QMenu(this);

    menu->addAction(EmojiIconProvider::createIcon("🏠"), "主屏幕", this, &DeviceView::onHomeScreenClicked);
    menu->addAction(EmojiIconProvider::createIcon("🧹"), "清理应用", this, &DeviceView::onKillAllAppClicked);
    menu->addAction(EmojiIconProvider::createIcon("📁"), "文件管理", this, &DeviceView::onFileClicked);
    menu->addAction(EmojiIconProvider::createIcon("⏺️"), "录屏", this, &DeviceView::onRecorderClicked);
    menu->addAction(EmojiIconProvider::createIcon("📱"), "应用列表", this, &DeviceView::onAppListClicked);

    menu->addAction(EmojiIconProvider::createIcon("📸"), "截图", this, &DeviceView::onScreenshotClicked);
    menu->addAction(EmojiIconProvider::createIcon("🔄"), "重启", this, &DeviceView::onRebootClicked);

    if (deviceInfo->lockedStatus)
        menu->addAction(EmojiIconProvider::createIcon("🔓"), "解锁", this, &DeviceView::onUnlockClicked);
    else
        menu->addAction(EmojiIconProvider::createIcon("🔒"), "锁屏", this, &DeviceView::onLockClicked);

    menu->addAction(EmojiIconProvider::createIcon("🗑️"), "清空相册", this, &DeviceView::onDeleteAllPhotosClicked);
    menu->addAction(EmojiIconProvider::createIcon("🔊"), "音量+", this, &DeviceView::onVolumeUpClicked);
    menu->addAction(EmojiIconProvider::createIcon("🔈"), "音量-", this, &DeviceView::onVolumeDownClicked);

    menu->addAction(EmojiIconProvider::createIcon("🔧"), "修改分组", [=]() {
        if (g_mainWindow->getTabs().count() <= 1) {
            new ToastWidget("请先右键点击标签页添加自定义分组", this);
            return;
        }

        BitMaskEditorDialog dialog(g_mainWindow->getTabs(), deviceInfo->groupMask, this);
        if (dialog.exec() != QDialog::Accepted) return;

        settings.setValue(deviceInfo->deviceId + "/groupMask", deviceInfo->groupMask);
    });

    return menu;
}

void DeviceView::contextMenuEvent(QContextMenuEvent *event)
{
    auto* menu = createContextMenu();
    menu->exec(event->globalPos());
    delete menu;
}

void DeviceView::dragEnterEvent(QDragEnterEvent *event)
{
    qDebugEx() << "dragEnterEvent";

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
    qDebugEx() << "dropEvent";

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

void DeviceView::resizeEvent(QResizeEvent *event)
{
    if (videoFrameWidget) {
        overlay->move(videoFrameWidget->pos());
        overlay->resize(videoFrameWidget->size());
    }

    QWidget::resizeEvent(event);
}
