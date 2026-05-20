#pragma once

#include "Logger.h"
#include "MainWindow.h"
#include "EventHub.h"
#include "DeviceWidget.h"
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsVideoItem>
#include <QMediaPlayer>
#include <QResizeEvent>
#include <QShowEvent>
#include <QBuffer>
#include <QPainter>
#include <QMouseEvent>
#include <QTimer>

class VideoFrameWidget : public QGraphicsView
{
    Q_OBJECT

public:
    explicit VideoFrameWidget(DeviceConnection* connection, QWidget *parent = nullptr) : connection(connection), QGraphicsView(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);

        auto scene = new QGraphicsScene(this);
        scene->setBackgroundBrush(Qt::black);
        setScene(scene);

        videoItem = new QGraphicsVideoItem();
        videoItem->setAspectRatioMode(Qt::AspectRatioMode::KeepAspectRatio);
        scene->addItem(videoItem);

        setFrameShape(QFrame::NoFrame);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setAlignment(Qt::AlignLeft | Qt::AlignTop);
        
        setMouseTracking(true); 

        mediaPlayer->setVideoOutput(videoItem);
        mediaPlayer->setAudioOutput(nullptr);

        connect(mediaPlayer, &QMediaPlayer::mediaStatusChanged, this, [=](QMediaPlayer::MediaStatus status) {
            qDebugEx() << "Media Status Changed: " << status;
            if (status == QMediaPlayer::LoadedMedia) {
                qDebugEx() << "播放...";
            }
        });

        connect(mediaPlayer, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error error, const QString &errorString) {
            qCriticalEx() << "errorOccurred" << error << errorString;
        });

        layoutTimer = new QTimer(this);
        layoutTimer->setSingleShot(true);
        layoutTimer->setInterval(10);
        connect(layoutTimer, &QTimer::timeout, this, &VideoFrameWidget::applyVideoSettings);

        EventHub::on(this, "orientation", [this](const QJsonValue &data, DeviceConnection* connection) {
            if (this->connection != connection)
                return;

            QResizeEvent re(size(), size());
            resizeEvent(&re);
        });
    }

    ~VideoFrameWidget() {
        EventHub::off(this, "orientation");
    }

    QByteArray grabFrame()
    {
        QRectF rect = videoItem->boundingRect();
        if (rect.isEmpty())
            return QByteArray();

        QImage image(rect.size().toSize(), QImage::Format_RGB32);
        image.fill(Qt::black);

        QPainter painter(&image);
        scene()->render(&painter, image.rect(), videoItem->sceneBoundingRect());
        painter.end();

        QByteArray byteArray;
        QBuffer buffer(&byteArray);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "JPG");

        return byteArray;
    }

    QMediaPlayer* const mediaPlayer = new QMediaPlayer(this);

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QGraphicsView::resizeEvent(event);
        layoutTimer->start();
    }

    void showEvent(QShowEvent *event) override
    {
        QGraphicsView::showEvent(event);
        layoutTimer->start();
    }

private:
    DeviceWidget* findOwnerWidget() const
    {
        for (QWidget *widget = parentWidget(); widget; widget = widget->parentWidget()) {
            if (auto *owner = qobject_cast<DeviceWidget *>(widget))
                return owner;
        }
        return nullptr;
    }

    void applyVideoLayout()
    {
        if (width() <= 0 || height() <= 0)
            return;

        const bool isPortrait = connection->deviceInfo->orientation == 1 || connection->deviceInfo->orientation == 2;
        const QSize containerSize = size();
        const QSize sourceSize(connection->deviceInfo->screenWidth, connection->deviceInfo->screenHeight);
        const QSize displaySourceSize = isPortrait ? sourceSize : sourceSize.transposed();

        const QSizeF targetSizeF = displaySourceSize.scaled(containerSize, Qt::KeepAspectRatio);
        videoItem->setSize(targetSizeF);
        scene()->setSceneRect(0, 0, containerSize.width(), containerSize.height());

        const qreal x = (containerSize.width() - targetSizeF.width()) / 2.0;
        const qreal y = (containerSize.height() - targetSizeF.height()) / 2.0;
        videoItem->setPos(x, y);
    }

    void applyVideoSettings()
    {
        applyVideoLayout();

        if (!isVisible() || width() <= 0 || height() <= 0)
            return;

        const auto isWindow = qobject_cast<DeviceWindow *>(parentWidget());
        if (!isWindow) {
            if (const auto *ownerWidget = findOwnerWidget()) {
                if (ownerWidget->getDeviceWindow()) {
                    qCriticalEx() << connection->deviceInfo->deviceName << "小视图此时不应发送设置";
                    return;
                }
            }
        }

        const bool isPortrait = connection->deviceInfo->orientation == 1 || connection->deviceInfo->orientation == 2;
        const QSize containerSize = size();
        const QSize sourceSize(connection->deviceInfo->screenWidth, connection->deviceInfo->screenHeight);
        const QSize displaySourceSize = isPortrait ? sourceSize : sourceSize.transposed();
        const QSizeF targetSizeF = displaySourceSize.scaled(containerSize, Qt::KeepAspectRatio);
        const QSize targetSize = targetSizeF.toSize();

        auto tab = MainWindow::getInstance()->getTab();
        auto videoFps = tab.getVideoFps();
        auto videoQuality = tab.getVideoQuality();

        const float dpr = devicePixelRatioF();
        const int physicalWidth = qRound(targetSize.width() * dpr);
        const int physicalHeight = qRound(targetSize.height() * dpr);
        const int minPhysicalRes = qMin(physicalWidth, physicalHeight);

        int minAllowed = 1;
        int maxAllowed = 4;

        if (minPhysicalRes < 180) maxAllowed = 1;
        else if (minPhysicalRes < 320) maxAllowed = 2;
        else if (minPhysicalRes < 480) maxAllowed = 3;
        else maxAllowed = 4;

        if (minPhysicalRes >= 1080) minAllowed = 3;
        else if (minPhysicalRes >= 720) minAllowed = 2;

        if (minAllowed > maxAllowed) minAllowed = maxAllowed;

        videoQuality = qBound(minAllowed, videoQuality, maxAllowed);

        const int finalWidth = physicalWidth;
        const int finalHeight = physicalHeight;

        connection->send("videoSettings", QJsonObject({
            {"width", qMin(finalWidth, finalHeight)},
            {"height", qMax(finalWidth, finalHeight)},
            {"fps", isWindow ? 30 : QList<float>{ 30, 0.2f, 1, 15, 30 }[qBound(0, videoFps, 4)]},
            {"quality", videoQuality}
        }));
    }

    DeviceConnection* connection;
    QGraphicsVideoItem* videoItem;
    QTimer* layoutTimer;
};
