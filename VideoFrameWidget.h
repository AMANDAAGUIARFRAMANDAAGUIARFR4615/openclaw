#pragma once

#include "Logger.h"
#include <QMediaPlayer>
#include <QGraphicsVideoItem>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QEvent>
#include <QVBoxLayout>

class VideoFrameWidget : public QGraphicsView
{
    Q_OBJECT

public:
    explicit VideoFrameWidget(QWidget *parent = nullptr) 
        : QGraphicsView(parent),
          mediaPlayer(new QMediaPlayer(this))
    {
        scene = new QGraphicsScene(this);
        setScene(scene);

        videoItem = new QGraphicsVideoItem();
        scene->addItem(videoItem);

        mediaPlayer->setVideoOutput(videoItem);
        mediaPlayer->setAudioOutput(nullptr);

        connect(mediaPlayer, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
            qDebugEx() << "Media Status Changed: " << status;
            if (status == QMediaPlayer::LoadedMedia) {
                qDebugEx() << "播放...";
            }
        });

        connect(mediaPlayer, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error error, const QString &errorString) {
            qCriticalEx() << "errorOccurred" << errorString;
        });

        QMetaObject::invokeMethod(this, [this]() {
            if (auto child = findChildWidget())
                child->installEventFilter(this);
            else
                qCriticalEx() << "QWindowContainer not found!";
        }, Qt::QueuedConnection);
    }

    void orientationChanged(int orientation)
    {
        auto width = size().width();
        auto height = size().height();

        if ((orientation == 1 || orientation == 2) && height < width || (orientation == 3 || orientation == 4) && height > width)
        {
            QString className = parentWidget()->metaObject()->className();

            if (className == "DeviceWindow")
                setFixedSize(height, width);
        }

        update();
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

        return QGraphicsView::eventFilter(obj, event);
    }

    QWidget* findChildWidget() const
    {
        for (QWidget *child : findChildren<QWidget*>()) {
            if (child->metaObject()->className() == QString("QWindowContainer"))
                return child;
        }

        return nullptr;
    }

private:
    QGraphicsScene *scene;
    QGraphicsVideoItem *videoItem;
};