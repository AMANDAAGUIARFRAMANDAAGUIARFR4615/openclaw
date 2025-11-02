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
    QLabel *label = new QLabel("----------------", overlay);
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
    auto window = new RemoteFileExplorer(connection);
    window->resize(deviceInfo->screenWidth * deviceInfo->scaleFactor, deviceInfo->screenHeight * deviceInfo->scaleFactor);
    window->show();
}

void DeviceView::onRecorderClicked()
{
    Recorder *recorder = new Recorder(connection);

    recorder->resize(920, 400);
    recorder->show();
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
    QMenu menu(this);

    auto homeAction = menu.addAction(QIcon(":/icons/home.png"), "主屏幕");
    connect(homeAction, &QAction::triggered, this, &DeviceView::onHomeScreenClicked);

    auto killAllAppAction = menu.addAction(QIcon(":/icons/kill.png"), "清理应用");
    connect(killAllAppAction, &QAction::triggered, this, &DeviceView::onKillAllAppClicked);

    auto fileAction = menu.addAction(QIcon(":/icons/file_move.png"), "文件管理");
    connect(fileAction, &QAction::triggered, this, &DeviceView::onFileClicked);

    auto recorderAction = menu.addAction(QIcon(":/icons/screen_record.png"), "录屏");
    connect(recorderAction, &QAction::triggered, this, &DeviceView::onRecorderClicked);

    auto appListAction = menu.addAction(QIcon(":/icons/apps.png"), "应用列表");
    connect(appListAction, &QAction::triggered, this, &DeviceView::onAppListClicked);

    if (deviceInfo->lockedStatus)
    {
        auto unlockAction = menu.addAction(QIcon(":/icons/unlock.png"), "解锁");
        connect(unlockAction, &QAction::triggered, this, &DeviceView::onUnlockClicked);
    }
    else
    {
        auto lockAction = menu.addAction(QIcon(":/icons/lock.png"), "锁屏");
        connect(lockAction, &QAction::triggered, this, &DeviceView::onLockClicked);
    }

    auto rebootAction = menu.addAction(QIcon(":/icons/restart.png"), "重启");
    connect(rebootAction, &QAction::triggered, this, &DeviceView::onRebootClicked);

    auto volumeUpAction = menu.addAction(QIcon(":/icons/volume_up.png"), "加音");
    connect(volumeUpAction, &QAction::triggered, this, &DeviceView::onVolumeUpClicked);

    auto volumeDownAction = menu.addAction(QIcon(":/icons/volume_down.png"), "减音");
    connect(volumeDownAction, &QAction::triggered, this, &DeviceView::onVolumeDownClicked);

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
