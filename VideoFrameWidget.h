#pragma once

#include "Logger.h"
#include "MainWindow.h"
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsVideoItem>
#include <QMediaPlayer>
#include <QResizeEvent>
#include <QBuffer>
#include <QPainter>

class VideoFrameWidget : public QGraphicsView
{
    Q_OBJECT

public:
    explicit VideoFrameWidget(DeviceConnection* connection, QWidget *parent = nullptr) : connection(connection), QGraphicsView(parent), mediaPlayer(new QMediaPlayer(this))
    {
        auto scene = new QGraphicsScene(this);
        setScene(scene);

        videoItem = new QGraphicsVideoItem();
        scene->addItem(videoItem);

        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        
        setFrameShape(QFrame::NoFrame);

        setBackgroundBrush(Qt::black);

        setAlignment(Qt::AlignCenter);

        connect(videoItem, &QGraphicsVideoItem::nativeSizeChanged, this, [=](const QSizeF &size){
            videoItem->setSize(size);
            fitInView(videoItem, Qt::KeepAspectRatio);
        });

        mediaPlayer->setVideoOutput(videoItem);
        mediaPlayer->setAudioOutput(nullptr);

        connect(mediaPlayer, &QMediaPlayer::mediaStatusChanged, this, [=](QMediaPlayer::MediaStatus status) {
            qDebugEx() << "Media Status Changed: " << status;
            if (status == QMediaPlayer::LoadedMedia) {
                qDebugEx() << "播放...";
                // 视频加载好后，强制刷新一次视图适应
                fitInView(videoItem, Qt::KeepAspectRatio);
            }
        });

        connect(mediaPlayer, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error error, const QString &errorString) {
            qCriticalEx() << "errorOccurred" << error << errorString;
        });
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

    void resizeEvent(QResizeEvent *event) override
    {
        QGraphicsView::resizeEvent(event);

        if (event->size().width() == 0 || event->size().height() == 0)
            return;

        // 宽度保持 16 字节对齐（很多编码器的硬性要求）
        int alignedWidth = (event->size().width() + 15) & ~15;
        // 使用 long long 防止乘法溢出，最后转回 int
        int alignedHeight = (int)((long long)alignedWidth * event->size().height() / event->size().width());

        // 高度强制设为偶数 (对齐到 2)
        // 视频编码通常要求宽高至少是 2 的倍数，奇数高度会导致崩溃或花屏
        alignedHeight = alignedHeight & ~1;

        alignedWidth *= devicePixelRatioF();
        alignedHeight *= devicePixelRatioF();

        auto tab = MainWindow::getInstance()->getTab();
        auto videoFps = tab.getVideoFps();
        auto videoQuality = tab.getVideoQuality();

        // 取宽高中较小的一边作为分辨率基准 (例如 1280x720 -> 720)
        int minResolution = qMin(alignedWidth, alignedHeight);

        int minAllowed = 1;
        int maxAllowed = 4;

        // --- 设定最大画质限制 (防止窗口太小浪费带宽) ---
        if (minResolution < 480)
            maxAllowed = 1; // 窗口极小，强制最低画质
        else if (minResolution < 720)
            maxAllowed = 2; // 窗口较小，限制中等画质
        else if (minResolution < 1080)
            maxAllowed = 3; // 窗口一般，限制高清
        else
            maxAllowed = 4; // 窗口很大，允许超清

        // --- 设定最小画质限制 (防止窗口太大导致模糊) ---
        // 注意：这里需要确保 min <= max，否则逻辑会冲突
        // 只有当窗口真的很大时，才强制提升最低画质
        if (minResolution >= 1200)
            minAllowed = 3; // 大屏下，至少要 3 档，否则全是马赛克
        else if (minResolution >= 800)
            minAllowed = 2; // 中屏下，至少要 2 档

        // 确保 min 不会超过 max (以 max 为主，即优先保证不卡顿/不浪费)
        if (minAllowed > maxAllowed)
            minAllowed = maxAllowed;

        videoQuality = qBound(minAllowed, videoQuality, maxAllowed);

        connection->send("videoSettings", QJsonObject({
            {"width", qMin(alignedWidth, alignedHeight)},
            {"height", qMax(alignedWidth, alignedHeight)},
            {"fps", QList<float>{ 1, 0.2f, 1, 15, 30 }[videoFps]},
            {"quality", videoQuality}
        }));
    }

    QMediaPlayer* const mediaPlayer;
    DeviceConnection* connection;

private:
    QGraphicsVideoItem* videoItem;
};