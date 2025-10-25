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
          mediaPlayer(nullptr),
          videoSink(nullptr),
          frameProcessor(nullptr), // 新增：帧处理器
          workerThread(new QThread(this))
    {
        // 初始化 QMediaPlayer、QVideoSink 和 VideoFrameProcessor
        mediaPlayer = new QMediaPlayer();
        videoSink = new QVideoSink();
        frameProcessor = new VideoFrameProcessor(); // 新增：创建帧处理器
        mediaPlayer->moveToThread(workerThread);
        videoSink->moveToThread(workerThread);
        frameProcessor->moveToThread(workerThread); // 新增：将帧处理器移到工作线程

        // 启动线程
        workerThread->start();

        // 设置视频输出
        mediaPlayer->setVideoSink(videoSink);

        // 连接视频帧信号到帧处理器
        connect(videoSink, &QVideoSink::videoFrameChanged, frameProcessor, &VideoFrameProcessor::processFrame, Qt::QueuedConnection);

        // 连接帧处理器到主线程的绘制
        connect(frameProcessor, &VideoFrameProcessor::frameProcessed, this, &VideoFrameWidget::onFrameProcessed, Qt::QueuedConnection);

        mediaPlayer->setAudioOutput(nullptr);

        // 连接媒体状态信号
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
        }, Qt::QueuedConnection);

        // 连接错误信号
        connect(mediaPlayer, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error error, const QString &errorString) {
            qCriticalEx() << "errorOccurred" << errorString;
        }, Qt::QueuedConnection);

        // 清理线程
        connect(this, &QObject::destroyed, workerThread, &QThread::quit);
        connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);
        connect(this, &QObject::destroyed, frameProcessor, &QObject::deleteLater); // 新增：清理帧处理器
    }

    ~VideoFrameWidget() override {
        if (workerThread->isRunning()) {
            workerThread->quit();
            workerThread->wait();
        }
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

    // 新增：处理帧处理器发来的 QImage
    void onFrameProcessed(const QImage &image)
    {
        currentImage = image;
        update();
    }

    QVideoSink *videoSink;
    QImage currentImage;
    int rotationAngle = 0;
    QThread *workerThread;
    VideoFrameProcessor *frameProcessor; // 新增：帧处理器
};
