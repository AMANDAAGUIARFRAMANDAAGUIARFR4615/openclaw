#pragma once

#include "DeviceView.h"
#include <QAudioOutput>

class DeviceWidget;

class DeviceWindow : public DeviceView
{
    Q_OBJECT
public:
    explicit DeviceWindow(DeviceConnection* connection, DeviceInfo* deviceInfo, DeviceWidget* deviceWidget);
    ~DeviceWindow();

    DeviceWidget* const deviceWidget;

protected:
    void changeOrientation(int orientation);
    void showEvent(QShowEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

    QMediaPlayer* audioPlayer = nullptr;
    DeviceConnection* audioDeviceConnection = nullptr;
};
