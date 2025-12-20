#pragma once

#include "Logger.h"
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsVideoItem>
#include <QMediaPlayer>
#include <QResizeEvent>

class VideoFrameWidget : public QGraphicsView
{
    Q_OBJECT

public:
    explicit VideoFrameWidget(QWidget *parent = nullptr) 
        : QGraphicsView(parent),
          mediaPlayer(new QMediaPlayer(this))
    {
        auto scene = new QGraphicsScene(this);
        setScene(scene);

        auto videoItem = new QGraphicsVideoItem();
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

    QMediaPlayer* const mediaPlayer;
};