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
    void addOverlay(const QString &text) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

    DeviceWindow* deviceWindow = nullptr;
    QSize videoFrameWidgetSize;

    friend DeviceWindow;
};
