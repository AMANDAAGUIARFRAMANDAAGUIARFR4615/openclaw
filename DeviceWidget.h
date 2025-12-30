#pragma once

#include "DeviceView.h"

class DeviceWindow;

class DeviceWidget : public DeviceView
{
    Q_OBJECT
public:
    explicit DeviceWidget(DeviceConnection* connection, DeviceInfo* deviceInfo);
    ~DeviceWidget();

protected:
    bool event(QEvent *event) override;
    void launchDeviceWindow();

    Q_INVOKABLE void play() {
        // static int count = 0;
        // count++;
        // qCriticalEx() << "播放" << count << this;
        if (videoFrameWidget)
            videoFrameWidget->mediaPlayer->play();
    }

    Q_INVOKABLE void pause() {
        // static int count = 0;
        // count++;
        // qCriticalEx() << "暂停" << count << this;
        if (videoFrameWidget)
            videoFrameWidget->mediaPlayer->pause();
    }

    DeviceWindow* deviceWindow = nullptr;
};
