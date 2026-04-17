#pragma once

#include "Logger.h"
#include "MainWindow.h"
#include "EventHub.h"
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsVideoItem>
#include <QMediaPlayer>
#include <QResizeEvent>
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

        // --- 防抖动逻辑 ---
        // 每次 resize 被触发时，重新启动定时器。
        // 如果布局调整连续触发了 4 次，前 3 次的计时会被重置。
        // 只有最后一次稳定下来后，定时器才会耗尽并执行 applyVideoSettings。
        layoutTimer->start();
    }

private:
    void applyVideoSettings()
    {
        if (!isVisible() || width() <= 0 || height() <= 0)
            return;

        bool isPortrait = connection->deviceInfo->orientation == 1 || connection->deviceInfo->orientation == 2;
        auto aspectRatio = isPortrait ? (float)connection->deviceInfo->screenHeight / connection->deviceInfo->screenWidth : (float)connection->deviceInfo->screenWidth / connection->deviceInfo->screenHeight;

        // 宽度保持 16 对齐（很多编码器的硬性要求）
        int alignedWidth = (width() + 15) & ~15;
        int alignedHeight = qRound((float)alignedWidth * aspectRatio);

        // 高度也强制 16 对齐，满足统一的编码分辨率要求
        alignedHeight = (alignedHeight + 15) & ~15;

        auto tab = MainWindow::getInstance()->getTab();
        auto videoFps = tab.getVideoFps();
        auto videoQuality = tab.getVideoQuality();

        // 使用物理像素计算清晰度限制，解决高分屏模糊问题
        float dpr = devicePixelRatioF();
        int physicalWidth = qRound(alignedWidth * dpr);
        int physicalHeight = qRound(alignedHeight * dpr);
        int minPhysicalRes = qMin(physicalWidth, physicalHeight);

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

        auto isWindow = qobject_cast<DeviceWindow*>(parentWidget());
        auto standaloneAlwaysUltraHD = isWindow && AppSettingsDialog::getInstance()->getValue("standaloneAlwaysUltraHD");

        // 确保 width 为较小边，height 为较大边，并进行整数四舍五入
        int finalWidth, finalHeight;
        if (standaloneAlwaysUltraHD) {
            finalWidth = qMin(connection->deviceInfo->screenWidth, connection->deviceInfo->screenHeight);
            finalHeight = qMax(connection->deviceInfo->screenWidth, connection->deviceInfo->screenHeight);
        } else {
            finalWidth = qMin(physicalWidth, physicalHeight);
            finalHeight = qMax(physicalWidth, physicalHeight);
        }

        // 确保宽高是 16 的倍数，避免编码侧因对齐不足出现异常
        finalWidth = (finalWidth + 15) & ~15;
        finalHeight = (finalHeight + 15) & ~15;

        connection->send("videoSettings", QJsonObject({
            {"width", finalWidth},
            {"height", finalHeight},
            {"fps", isWindow ? 30 : QList<float>{ 30, 0.2f, 1, 15, 30, 60, 120 }[qBound(0, videoFps, 6)]},
            {"quality", videoQuality}
        }));

        QSize containerSize = size();
        QSize sourceSize(connection->deviceInfo->screenWidth, connection->deviceInfo->screenHeight);

        QSizeF targetSize = (isPortrait ? sourceSize : sourceSize.transposed()).scaled(containerSize, Qt::KeepAspectRatio);

        videoItem->setSize(targetSize);

        scene()->setSceneRect(0, 0, containerSize.width(), containerSize.height());

        qreal x = (containerSize.width() - targetSize.width()) / 2.0;
        qreal y = (containerSize.height() - targetSize.height()) / 2.0;
        
        videoItem->setPos(x, y);
    }

    DeviceConnection* connection;
    QGraphicsVideoItem* videoItem;
    QTimer* layoutTimer;
};
