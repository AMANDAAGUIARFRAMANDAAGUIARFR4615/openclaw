#pragma once

#include "Logger.h"
#include "DeviceConnection.h"
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QEvent>
#include <QResizeEvent>

class VideoFrameWidget : public QVideoWidget
{
    Q_OBJECT

public:
    explicit VideoFrameWidget(DeviceConnection* connection, QWidget *parent = nullptr) : connection(connection), QVideoWidget(parent), mediaPlayer(new QMediaPlayer(this))
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

    void resizeEvent(QResizeEvent *event) override
    {
        QVideoWidget::resizeEvent(event);

        // 宽度保持 16 字节对齐（很多编码器的硬性要求）
        int alignedWidth = (event->size().width() + 15) & ~15;

        QSize sourceSize = frameSize(); 
        
        int alignedHeight = event->size().height();

        if (!sourceSize.isEmpty()) {
            // 使用 long long 防止乘法溢出，最后转回 int
            alignedHeight = (int)((long long)alignedWidth * sourceSize.height() / sourceSize.width());
        }

        // 高度强制设为偶数 (对齐到 2)
        // 视频编码通常要求宽高至少是 2 的倍数，奇数高度会导致崩溃或花屏
        alignedHeight = alignedHeight & ~1;

        connection->send("videoSettings", QJsonObject({
            {"width", alignedWidth},
            {"height", alignedHeight},
            {"fps", 0.2},
            {"quality", 2}
        }));
    }

    DeviceConnection* connection;
};
