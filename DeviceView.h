#pragma once

#include "DeviceInfo.h"
#include "DeviceConnection.h"
#include "VideoFrameWidget.h"
#include <QWidget>
#include <QUrl>
#include <QMenu>

class QMediaPlayer;

class DeviceView : public QWidget
{
    Q_OBJECT
public:
    explicit DeviceView(DeviceConnection* connection, DeviceInfo* deviceInfo, QWidget *parent = nullptr);
    ~DeviceView();

    void setSourceDevice(QIODevice *device, const QUrl &sourceUrl = QUrl());

    void onHomeScreenClicked();
    void onCenterControllerClicked();
    void onKillAllAppClicked();
    void onAppSwitcherClicked();
    void onFileClicked();
    void onRecorderClicked();
    void onAppListClicked();
    void onToggleMultiControl();
    void onScreenshotClicked();
    void onRebootClicked();
    void onDeleteAllPhotosClicked();
    void onLockClicked();
    void onUnlockClicked();
    void onVolumeUpClicked();
    void onVolumeDownClicked();

    void addVideoFrameWidget(VideoFrameWidget* videoFrameWidget);

protected:
    void addOverlay(const QString &text);
    virtual QMenu* createContextMenu();
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    
    QPoint getTransformedPosition(QPoint pos);
    bool event(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

    DeviceConnection* const connection;
    DeviceInfo* const deviceInfo;
    VideoFrameWidget *videoFrameWidget = nullptr;
    QWidget *overlay = nullptr;

    bool isMultiControlEnabled = false;

    Qt::MouseButtons pressedButtons = Qt::NoButton;
    QTimer *wheelTimer = nullptr;
    QPoint currentPos;
    int accumulatedDelta = 0;
    const int maxSteps = 5;
    int stepCount = 0;
};
