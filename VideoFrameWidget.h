#pragma once

#include "Logger.h"
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QEvent>

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

        connect(mediaPlayer, &QMediaPlayer::mediaStatusChanged, [this](QMediaPlayer::MediaStatus status) {
            qDebugEx() << "Media Status Changed: " << status;
            if (status == QMediaPlayer::LoadedMedia) {
                qDebugEx() << "播放...";
            }
        });

        connect(mediaPlayer, &QMediaPlayer::errorOccurred, [this](QMediaPlayer::Error error, const QString &errorString) {
            qCriticalEx() << "errorOccurred" << error << errorString;
        });

        QMetaObject::invokeMethod(this, [this]() {
            if (auto child = findChildWidget())
                child->installEventFilter(this);
            else
                qCriticalEx() << "QWindowContainer not found!";
        }, Qt::QueuedConnection);
    }

    QMediaPlayer* const mediaPlayer;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override
    {
        switch (event->type()) {
        case QEvent::DragEnter:
        case QEvent::DragMove:
        case QEvent::Drop:
            return true;
        }

        return QVideoWidget::eventFilter(obj, event);
    }

    QWidget* findChildWidget() const
    {
        for (QWidget *child : findChildren<QWidget*>()) {
            if (child->metaObject()->className() == QString("QWindowContainer"))
                return child;
        }

        return nullptr;
    }
};
