#include "DeviceView.h"
#include "Logger.h"
#include "DeviceWindow.h"
#include "Tools.h"
#include "RemoteFileExplorer.h"
#include "TcpServer.h"
#include "FileTransfer.h"
#include "EventHub.h"
#include <QLayout>
#include <QMenu>
#include <QLabel>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QDir>
#include <QUuid>

DeviceView::DeviceView(QTcpSocket* socket, DeviceInfo* deviceInfo, QWidget *parent)
    : socket(socket), deviceInfo(deviceInfo), QWidget(parent)
{
    setAcceptDrops(true);

    EventHub::StartListening("orientation", [this](const QJsonValue &data, QTcpSocket* socket) {
        if (this->socket != socket)
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
    mediaSource = source;
    
    if (deviceInfo->lockedStatus)
        addOverlay("设备已锁定");
    else
        addVideoFrameWidget(new VideoFrameWidget(this));
}

void DeviceView::addOverlay(const QString &text)
{
    if (videoFrameWidget)
    {
        videoFrameWidget->deleteLater();
        videoFrameWidget = nullptr;
    }

    overlay = new QWidget(this);
    overlay->setStyleSheet("background-color: black;");
    QLabel *label = new QLabel(text, overlay);
    label->setStyleSheet("color: white; font-size: 20px;");
    label->setAlignment(Qt::AlignCenter);

    QVBoxLayout *layout = new QVBoxLayout(overlay);
    layout->addStretch();
    layout->addWidget(label);
    layout->addStretch();
    overlay->setLayout(layout);

    this->layout()->addWidget(overlay);
}

void DeviceView::addVideoFrameWidget(VideoFrameWidget* videoFrameWidget)
{
    if (overlay)
    {
        overlay->deleteLater();
        overlay = nullptr;
    }

    this->videoFrameWidget = videoFrameWidget;
    layout()->addWidget(videoFrameWidget);
    videoFrameWidget->mediaPlayer->setSource(mediaSource);
    videoFrameWidget->orientationChanged(deviceInfo->orientation);
}

void DeviceView::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu contextMenu(this);

    QAction *fileAction = new QAction("文件传输", this);
    connect(fileAction, &QAction::triggered, [this]() {
        auto window = new RemoteFileExplorer(socket);
        window->resize(deviceInfo->screenWidth * deviceInfo->scaleFactor, deviceInfo->screenHeight * deviceInfo->scaleFactor);
        window->show();
    });

    QAction *unlockAction = new QAction("解锁", this);
    connect(unlockAction, &QAction::triggered, [this]() {
        Tools::unlockScreen(socket);
    });

    QAction *lockAction = new QAction("锁屏", this);
    connect(lockAction, &QAction::triggered, [this]() {
        Tools::lockScreen(socket);
    });

    QAction *rebootAction = new QAction("重启", this);
    connect(rebootAction, &QAction::triggered, [this]() {
        Tools::reboot(socket);
    });

    QAction *volumeUpAction = new QAction("音量+", this);
    connect(volumeUpAction, &QAction::triggered, [this]() {
        QJsonObject jsonObject;
        jsonObject["event"] = "volumeControl";
        jsonObject["data"] = "+";

        TcpServer::sendData(socket, jsonObject);
    });

    QAction *volumeDownAction = new QAction("音量-", this);
    connect(volumeDownAction, &QAction::triggered, [this]() {
        QJsonObject jsonObject;
        jsonObject["event"] = "volumeControl";
        jsonObject["data"] = "-";

        TcpServer::sendData(socket, jsonObject);
    });

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
        auto id = QUuid::createUuid().toString();
        auto type = 2; // 收是1，发是2
        auto path = url.toLocalFile();
        auto size = Tools::getFileSize(path);
        
        auto transfer = new FileTransfer(type, path, size);

        QJsonObject dataObject;
        dataObject["id"] = id;
        dataObject["type"] = type;
        dataObject["port"] = transfer->serverPort();
        dataObject["name"] = QFileInfo(path).fileName();
        dataObject["size"] = size;

        QJsonObject jsonObject;
        jsonObject["event"] = "transferFile";
        jsonObject["data"] = dataObject;

        TcpServer::sendData(socket, jsonObject);
    }

    event->accept();
}
