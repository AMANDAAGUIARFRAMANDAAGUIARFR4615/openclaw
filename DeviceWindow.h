#pragma once

#include "DeviceView.h"
#include <QAudioOutput>

class DeviceWidget;

class DeviceWindow : public DeviceView
{
    Q_OBJECT
public:
    explicit DeviceWindow(DeviceConnection* connection, DeviceInfo* deviceInfo);
    ~DeviceWindow();

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    QMenu* createContextMenu() override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

    QMediaPlayer* audioPlayer = nullptr;
    DeviceConnection* audioDeviceConnection = nullptr;
};
