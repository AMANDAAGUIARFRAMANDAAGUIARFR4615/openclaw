#pragma once

#include "Logger.h"
#include <QMediaPlayer>
#include <QVideoSink>
#include <QVideoFrame>
#include <QImage>
#include <QPainter>
#include <QDebug>
#include <QApplication>
#include <QWidget>
#include <QTimer>
#include <QDragEnterEvent>
#include <QDropEvent>

class VideoFrameWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoFrameWidget(QWidget *parent = nullptr) 
        : QWidget(parent), mediaPlayer(new QMediaPlayer(this))
    {
        videoSink = new QVideoSink(this);
        mediaPlayer->setVideoSink(videoSink);

        connect(videoSink, &QVideoSink::videoFrameChanged, this, &VideoFrameWidget::onVideoFrameChanged);

        mediaPlayer->setAudioOutput(nullptr);

        connect(mediaPlayer, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
            qDebugEx() << "Media Status Changed: " << status;
            if (status == QMediaPlayer::LoadedMedia) {
                if (!mediaPlayer->isPlaying() && mediaPlayer->playbackState() != QMediaPlayer::PausedState) {
                    qDebugEx() << "播放...";
                    if (!mediaPlayer->sourceDevice())
                        mediaPlayer->stop();

                    mediaPlayer->play();
                }
            }
        });

        connect(mediaPlayer, &QMediaPlayer::errorChanged, [this]() {
            qCriticalEx() << "errorChanged" << mediaPlayer->errorString();
        });
    }

    void orientationChanged(int orientation)
    {
        switch (orientation) {
            case 1: // Portrait
                rotationAngle = 0;
                break;
            case 2: // PortraitUpsideDown
                rotationAngle = 180;
                break;
            case 3: // LandscapeRight
                rotationAngle = -90;
                break;
            case 4: // LandscapeLeft
                rotationAngle = 90;
                break;
        }

        auto width = size().width();
        auto height = size().height();

        if ((orientation == 1 || orientation == 2) && height < width || (orientation == 3 || orientation == 4) && height > width)
        {
            QString className = parentWidget()->metaObject()->className();

            if (className == "DeviceWindow")
                setFixedSize(height, width);
            else
                resize(height, width);
        }
        else
        {
            update();
        }
    }

    QMediaPlayer* const mediaPlayer;

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QWidget::paintEvent(event);

        if (currentImage.isNull())
            return;

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

        QTransform transform;
        transform.rotate(rotationAngle);

        auto rotatedImage = currentImage.transformed(transform);
        auto scaled = rotatedImage.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);

        // 计算居中位置
        QRect targetRect(QPoint(0, 0), scaled.size());
        targetRect.moveCenter(rect().center());

        painter.drawImage(targetRect.topLeft(), scaled);
    }

    void onVideoFrameChanged(const QVideoFrame &frame)
    {
        if (!frame.isValid())
        {
            qCriticalEx() << "无效帧";
            return;
        }

        QImage img = frame.toImage();
        if (img.isNull())
        {
            qCriticalEx() << "frame.toImage()";
            return;
        }

        currentImage = img;
        update();
    }

    QVideoSink *videoSink;
    QImage currentImage;
    int rotationAngle = 0;
};
