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
#include <QThread>
#include <QObject>

// 新增：帧处理器类，运行在工作线程中
class VideoFrameProcessor : public QObject
{
    Q_OBJECT
public:
    explicit VideoFrameProcessor(QObject *parent = nullptr) : QObject(parent) {}

public slots:
    void processFrame(const QVideoFrame &frame) {
        if (!frame.isValid()) {
            qCriticalEx() << "无效帧";
            return;
        }

        QImage img = frame.toImage();
        if (img.isNull()) {
            qCriticalEx() << "frame.toImage()";
            return;
        }

        emit frameProcessed(img); // 发送处理后的 QImage
    }

signals:
    void frameProcessed(const QImage &image);
};

class VideoFrameWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoFrameWidget(QWidget *parent = nullptr)
        : QWidget(parent),
          workerThread(new QThread(this))
    {
        mediaPlayer = new QMediaPlayer(this);
        auto videoSink = new QVideoSink(this);
        auto frameProcessor = new VideoFrameProcessor(this);
        mediaPlayer->moveToThread(workerThread);
        videoSink->moveToThread(workerThread);
        frameProcessor->moveToThread(workerThread);

        workerThread->start();

        mediaPlayer->setVideoSink(videoSink);
        mediaPlayer->setAudioOutput(nullptr);

        connect(videoSink, &QVideoSink::videoFrameChanged, frameProcessor, &VideoFrameProcessor::processFrame, Qt::QueuedConnection);
        connect(frameProcessor, &VideoFrameProcessor::frameProcessed, this, &VideoFrameWidget::onFrameProcessed, Qt::QueuedConnection);

        connect(mediaPlayer, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
            qDebugEx() << "Media Status Changed: " << status;
            if (status == QMediaPlayer::LoadedMedia) {
                qDebugEx() << "播放...";
            }
        }, Qt::QueuedConnection);

        connect(mediaPlayer, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error error, const QString &errorString) {
            qCriticalEx() << "errorOccurred" << error << errorString;
        }, Qt::QueuedConnection);
    }

    ~VideoFrameWidget() override {
        if (workerThread->isRunning()) {
            workerThread->quit();
            workerThread->wait();
        }
    }

    QMediaPlayer *mediaPlayer;

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

        QRect targetRect(QPoint(0, 0), scaled.size());
        targetRect.moveCenter(rect().center());

        painter.drawImage(targetRect.topLeft(), scaled);
    }

    void onFrameProcessed(const QImage &image)
    {
        currentImage = image;
        update();
    }

    QImage currentImage;
    int rotationAngle = 0;
    QThread *workerThread;
};
