#pragma once

#include "DeviceInfo.h"
#include "VideoFrameWidget.h"
#include <QWidget>
#include <QUrl>
#include <QTcpSocket>

class QMediaPlayer;

class DeviceView : public QWidget
{
    Q_OBJECT
public:
    explicit DeviceView(QTcpSocket* socket, DeviceInfo* deviceInfo, QWidget *parent = nullptr);
    ~DeviceView();

    VideoFrameWidget* getVideoFrameWidget() { return videoFrameWidget; }
    void setSource(const QUrl &source);

    void onHomeScreenClicked();
    void onCenterControllerClicked();
    void onKillAllAppClicked();
    void onAppSwitcherClicked();
    void onFileClicked();
    void onScreenshotClicked();
    void onRestartClicked();
    void onLockClicked();
    void onUnlockClicked();
    void onVolumeUpClicked();
    void onVolumeDownClicked();

protected:
    virtual void addOverlay(const QString &text);
    virtual void addVideoFrameWidget(VideoFrameWidget* videoFrameWidget);

    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

    QTcpSocket* const socket;
    DeviceInfo* const deviceInfo;
    VideoFrameWidget *videoFrameWidget = nullptr;
    QUrl mediaSource;
    QWidget *overlay = nullptr;
};
