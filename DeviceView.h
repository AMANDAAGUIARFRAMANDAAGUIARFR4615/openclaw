#pragma once

#include "DeviceInfo.h"
#include "DeviceConnection.h"
#include "VideoFrameWidget.h"
#include <QWidget>
#include <QUrl>

class QMediaPlayer;

class DeviceView : public QWidget
{
    Q_OBJECT
public:
    explicit DeviceView(DeviceConnection* connection, DeviceInfo* deviceInfo, QWidget *parent = nullptr);
    ~DeviceView();

    VideoFrameWidget* getVideoFrameWidget() { return videoFrameWidget; }
    void setSource(const QUrl &source);

    void onHomeScreenClicked();
    void onCenterControllerClicked();
    void onKillAllAppClicked();
    void onAppSwitcherClicked();
    void onFileClicked();
    void onScreenshotClicked();
    void onRebootClicked();
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

    DeviceConnection* const connection;
    DeviceInfo* const deviceInfo;
    VideoFrameWidget *videoFrameWidget = nullptr;
    QUrl mediaSource;
    QWidget *overlay = nullptr;
};
