#pragma once

#include "Logger.h"
#include <QMediaPlayer>
#include <QVideoWidget>

class VideoFrameWidget : public QVideoWidget
{
    Q_OBJECT

public:
    explicit VideoFrameWidget(QWidget *parent = nullptr) 
        : QVideoWidget(parent),
          mediaPlayer(new QMediaPlayer(this))
    {
        mediaPlayer->setVideoOutput(this);
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

        connect(mediaPlayer, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error error, const QString &errorString) {
            qCriticalEx() << "errorOccurred" << errorString;
        });
    }

    void orientationChanged(int orientation)
    {
        switch (orientation) {
            case 1: // Portrait
                setProperty("rotation", 0);
                break;
            case 2: // PortraitUpsideDown
                setProperty("rotation", 180);
                break;
            case 3: // LandscapeRight
                setProperty("rotation", -90);
                break;
            case 4: // LandscapeLeft
                setProperty("rotation", 90);
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
        update();
    }

    QMediaPlayer* const mediaPlayer;
};
