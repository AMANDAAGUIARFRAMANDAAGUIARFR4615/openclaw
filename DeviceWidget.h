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
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void launchDeviceWindow();

    DeviceWindow* deviceWindow = nullptr;

    friend DeviceWindow;
};
