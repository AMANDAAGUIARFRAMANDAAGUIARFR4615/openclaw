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
    void setSourceDevice(QIODevice *device, const QUrl &sourceUrl = QUrl());

    void onHomeScreenClicked();
    void onCenterControllerClicked();
    void onKillAllAppClicked();
    void onAppSwitcherClicked();
    void onFileClicked();
    void onRecorderClicked();
    void onAppListClicked();
    void onScreenshotClicked();
    void onRebootClicked();
    void onLockClicked();
    void onUnlockClicked();
    void onVolumeUpClicked();
    void onVolumeDownClicked();

    virtual void addOverlay(const QString &text);
    virtual void addVideoFrameWidget(VideoFrameWidget* videoFrameWidget);

    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

protected:

    DeviceConnection* const connection;
    DeviceInfo* const deviceInfo;
    VideoFrameWidget *videoFrameWidget = nullptr;
    QWidget *overlay = nullptr;
};
