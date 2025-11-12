#include "DeviceView.h"
#include "Logger.h"
#include "DeviceWindow.h"
#include "Tools.h"
#include "RemoteFileExplorer.h"
#include "TcpServer.h"
#include "FileTransfer.h"
#include "EventHub.h"
#include "Recorder.h"
#include "AppListWidget.h"
#include "BitMaskEditorDialog.h"
#include "MainWindow.h"
#include <QLayout>
#include <QMenu>
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
    // 要多设置一次才能播放
    QTimer::singleShot(2000, [=]() {
        mediaPlayer->stop();
        mediaPlayer->play();
    });
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

void DeviceView::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);

    connect(menu.addAction(QIcon(":/icons/home.png"), "主屏幕"), &QAction::triggered, this, &DeviceView::onHomeScreenClicked);
    connect(menu.addAction(QIcon(":/icons/kill.png"), "清理应用"), &QAction::triggered, this, &DeviceView::onKillAllAppClicked);
    connect(menu.addAction(QIcon(":/icons/file_move.png"), "文件管理"), &QAction::triggered, this, &DeviceView::onFileClicked);
    connect(menu.addAction(QIcon(":/icons/screen_record.png"), "录屏"), &QAction::triggered, this, &DeviceView::onRecorderClicked);
    connect(menu.addAction(QIcon(":/icons/apps.png"), "应用列表"), &QAction::triggered, this, &DeviceView::onAppListClicked);

    if (deviceInfo->lockedStatus)
        connect(menu.addAction(QIcon(":/icons/unlock.png"), "解锁"), &QAction::triggered, this, &DeviceView::onUnlockClicked);
    else
        connect(menu.addAction(QIcon(":/icons/lock.png"), "锁屏"), &QAction::triggered, this, &DeviceView::onLockClicked);

    connect(menu.addAction(QIcon(":/icons/restart.png"), "重启"), &QAction::triggered, this, &DeviceView::onRebootClicked);
    connect(menu.addAction(QIcon(":/icons/photo.png"), "清空相册"), &QAction::triggered, this, &DeviceView::onDeleteAllPhotosClicked);
    connect(menu.addAction(QIcon(":/icons/volume_up.png"), "音量+"), &QAction::triggered, this, &DeviceView::onVolumeUpClicked);
    connect(menu.addAction(QIcon(":/icons/volume_down.png"), "音量-"), &QAction::triggered, this, &DeviceView::onVolumeDownClicked);

    connect(menu.addAction(QIcon(":/icons/category.png"), "修改分组"), &QAction::triggered, this, [=]() {
        if (g_mainWindow->getTabs().count() <= 1) {
            new ToastWidget("请先右键点击标签页添加自定义分组", this);
            return;
        }

        BitMaskEditorDialog dialog(g_mainWindow->getTabs(), deviceInfo->groupMask, this);
        if (dialog.exec() != QDialog::Accepted) return;

        settings.setValue(deviceInfo->deviceId + "/groupMask", deviceInfo->groupMask);
    });

    auto subMenu = menu.addMenu(QIcon(":/icons/high_quality.png"), "清晰度");
    connect(subMenu->addAction("低清"), &QAction::triggered, this, [this]() {
        connection->send("setVideoQuality", 1);
    });
    connect(subMenu->addAction("标清"), &QAction::triggered, this, [this]() {
        connection->send("setVideoQuality", 2);
    });
    connect(subMenu->addAction("高清"), &QAction::triggered, this, [this]() {
        connection->send("setVideoQuality", 3);
    });
    connect(subMenu->addAction("超清"), &QAction::triggered, this, [this]() {
        connection->send("setVideoQuality", 4);
    });

    menu.exec(event->globalPos());
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
