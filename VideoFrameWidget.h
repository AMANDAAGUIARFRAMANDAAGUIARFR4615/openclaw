#pragma once

#include "Logger.h"
#include <QWidget>
#include <QMediaPlayer>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsVideoItem>
#include <QVBoxLayout>
#include <QDebug>

class VideoFrameWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoFrameWidget(QWidget *parent = nullptr)
        : QWidget(parent)
        , mediaPlayer(new QMediaPlayer(this))
        , graphicsView(new QGraphicsView(this))
        , scene(new QGraphicsScene(this))
        , videoItem(new QGraphicsVideoItem)
    {
        // ---------- 1. 布局 ----------
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(graphicsView);
        setLayout(layout);

        // ---------- 2. QGraphicsView/Scene ----------
        graphicsView->setScene(scene);
        graphicsView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        graphicsView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        graphicsView->setStyleSheet("background: black;"); // 防止出现白边

        // ---------- 3. QGraphicsVideoItem ----------
        scene->addItem(videoItem);
        mediaPlayer->setVideoOutput(videoItem);
        mediaPlayer->setAudioOutput(nullptr); // 如不需要音频可关闭

        // ---------- 4. 视频尺寸自适应 ----------
        connect(mediaPlayer, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
            qDebugEx() << "Media Status Changed:" << status;
            if (status == QMediaPlayer::LoadedMedia) {
                // 视频加载完毕后自动播放（如果当前不在播放/暂停状态）
                if (mediaPlayer->playbackState() != QMediaPlayer::PlayingState &&
                    mediaPlayer->playbackState() != QMediaPlayer::PausedState) {
                    qDebugEx() << "开始播放...";
                    mediaPlayer->play();
                }
                // 自动适配视频原始尺寸
                resizeVideoToNativeSize();
            }
        });

        connect(mediaPlayer, &QMediaPlayer::errorOccurred, this,
                [this](QMediaPlayer::Error error, const QString &errorString) {
            qCriticalEx() << "MediaPlayer error:" << errorString;
        });

        // 视频帧尺寸变化时重新布局
        connect(videoItem, &QGraphicsVideoItem::nativeSizeChanged, this, &VideoFrameWidget::resizeVideoToNativeSize);
    }

    /** 公开的 QMediaPlayer，外部可以直接调用 play/pause/setSource 等 */
    QMediaPlayer * const mediaPlayer;

    /** 横竖屏切换时调用，保持视频宽高比 */
    void orientationChanged(int orientation)
    {
        Q_UNUSED(orientation);
        // 这里保持视频宽高比不变，只需要重新适配容器大小即可
        resizeVideoToNativeSize();
        update();
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        resizeVideoToNativeSize();
    }

private:
    /** 根据视频原始尺寸和当前 widget 大小进行等比缩放 */
    void resizeVideoToNativeSize()
    {
        const QSize videoSize = videoItem->nativeSize().toSize();
        if (videoSize.isEmpty()) {
            // 还未获取到尺寸，直接占满
            videoItem->setSize(size());
            scene->setSceneRect(0, 0, width(), height());
            return;
        }

        // 计算等比缩放后的大小
        const QRectF viewRect = graphicsView->rect();
        const qreal viewRatio = viewRect.width() / viewRect.height();
        const qreal videoRatio = static_cast<qreal>(videoSize.width()) / videoSize.height();

        QSizeF targetSize;
        if (viewRatio > videoRatio) {
            // 视图更宽，以高度为准
            targetSize.setHeight(viewRect.height());
            targetSize.setWidth(viewRect.height() * videoRatio);
        } else {
            // 视图更高，以宽度为准
            targetSize.setWidth(viewRect.width());
            targetSize.setHeight(viewRect.width() / videoRatio);
        }

        videoItem->setSize(targetSize);

        // 居中显示
        const QRectF itemRect = videoItem->boundingRect();
        const QRectF centeredRect(
            (viewRect.width()  - itemRect.width())  / 2,
            (viewRect.height() - itemRect.height()) / 2,
            itemRect.width(),
            itemRect.height()
        );
        videoItem->setPos(centeredRect.topLeft());

        scene->setSceneRect(viewRect);
    }

private:
    QGraphicsView     *graphicsView;
    QGraphicsScene    *scene;
    QGraphicsVideoItem *videoItem;
};