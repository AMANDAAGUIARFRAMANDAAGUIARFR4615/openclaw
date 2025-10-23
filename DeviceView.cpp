#include "DeviceView.h"
#include "Logger.h"
#include "DeviceWindow.h"
#include "Tools.h"
#include "RemoteFileExplorer.h"
#include "TcpServer.h"
#include "FileTransfer.h"
#include "EventHub.h"
#include "AppListWidget.h"
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
    setAcceptDrops(true);

    overlay = new QWidget(this);
    overlay->setStyleSheet("background-color: black;");
    QLabel *label = new QLabel("设备已锁定", overlay);
    label->setStyleSheet("color: white; font-size: 20px;");
    label->setAlignment(Qt::AlignCenter);

    QVBoxLayout *layout = new QVBoxLayout(overlay);
    layout->addStretch();
    layout->addWidget(label);
    layout->addStretch();
    overlay->setLayout(layout);

    EventHub::StartListening("orientation", [this](const QJsonValue &data, DeviceConnection* connection) {
        if (this->connection != connection)
            return;

        this->deviceInfo->orientation = data.toInt();

        if (videoFrameWidget)
            videoFrameWidget->orientationChanged(data.toInt());
    });
}

DeviceView::~DeviceView()
{

}

void DeviceView::setSource(const QUrl &source)
{
    addVideoFrameWidget(new VideoFrameWidget(this));

    if (deviceInfo->lockedStatus)
        addOverlay("设备已锁定");

    videoFrameWidget->mediaPlayer->setSource(source);
}

void DeviceView::setSourceDevice(QIODevice *device, const QUrl &sourceUrl)
{
    addVideoFrameWidget(new VideoFrameWidget(this));

    if (deviceInfo->lockedStatus)
        addOverlay("设备已锁定");
        
    videoFrameWidget->mediaPlayer->setSourceDevice(device);
    // 要多设置一次才能播放
    QTimer::singleShot(500, [=]() {
        videoFrameWidget->mediaPlayer->setSourceDevice(device);
    });
}

void DeviceView::addOverlay(const QString &text)
{
    QLabel *label = overlay->findChild<QLabel *>();
    label->setText(text);

    overlay->show();
    
    layout()->removeWidget(overlay);
    layout()->addWidget(overlay);
}

void DeviceView::addVideoFrameWidget(VideoFrameWidget* widget)
{
    overlay->hide();

    videoFrameWidget = widget;
    layout()->addWidget(widget);
    
    videoFrameWidget->orientationChanged(deviceInfo->orientation);
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
    auto window = new RemoteFileExplorer(connection);
    window->resize(deviceInfo->screenWidth * deviceInfo->scaleFactor, deviceInfo->screenHeight * deviceInfo->scaleFactor);
    window->show();
}

void DeviceView::onAppListClicked()
{
    connection->send("appList");

    AppListWidget *list = new AppListWidget(connection);

    list->resize(920, 400);
    list->show();
}

void DeviceView::onScreenshotClicked()
{
    connection->send("screenshot");
}

void DeviceView::onRebootClicked()
{
    connection->send("reboot");
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
    QMenu contextMenu(this);

    QAction *homeAction = new QAction(QIcon(":/icons/home.png"), "主屏幕", this);
    connect(homeAction, &QAction::triggered, this, &DeviceView::onHomeScreenClicked);

    QAction *killAllAppAction = new QAction(QIcon(":/icons/kill.png"), "清理应用", this);
    connect(killAllAppAction, &QAction::triggered, this, &DeviceView::onKillAllAppClicked);

    QAction *fileAction = new QAction(QIcon(":/icons/file_move.png"), "文件管理", this);
    connect(fileAction, &QAction::triggered, this, &DeviceView::onFileClicked);

    QAction *unlockAction = new QAction(QIcon(":/icons/unlock.png"), "解锁", this);
    connect(unlockAction, &QAction::triggered, this, &DeviceView::onUnlockClicked);

    QAction *lockAction = new QAction(QIcon(":/icons/lock.png"), "锁屏", this);
    connect(lockAction, &QAction::triggered, this, &DeviceView::onLockClicked);

    QAction *rebootAction = new QAction(QIcon(":/icons/restart.png"), "重启", this);
    connect(rebootAction, &QAction::triggered, this, &DeviceView::onRebootClicked);

    QAction *volumeUpAction = new QAction(QIcon(":/icons/volume_up.png"), "加音", this);
    connect(volumeUpAction, &QAction::triggered, this, &DeviceView::onVolumeUpClicked);

    QAction *volumeDownAction = new QAction(QIcon(":/icons/volume_down.png"), "减音", this);
    connect(volumeDownAction, &QAction::triggered, this, &DeviceView::onVolumeDownClicked);

    contextMenu.addAction(homeAction);
    contextMenu.addAction(killAllAppAction);
    contextMenu.addAction(fileAction);
    contextMenu.addAction(unlockAction);
    contextMenu.addAction(lockAction);
    contextMenu.addAction(rebootAction);
    contextMenu.addAction(volumeUpAction);
    contextMenu.addAction(volumeDownAction);

    contextMenu.exec(event->globalPos());
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
        auto path = url.toLocalFile();
        auto size = Tools::getFileSize(path);
        
        auto transfer = new FileTransfer(connection, type, path, size);

        QJsonObject dataObject;
        dataObject["type"] = type;
        dataObject["port"] = transfer->serverPort();
        dataObject["name"] = QFileInfo(path).fileName();
        dataObject["size"] = size;
        dataObject["id"] = path;

        connection->send("transferFile", dataObject);
    }

    event->accept();
}
