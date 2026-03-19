#pragma once

#include "DeviceView.h"
#include "LiveStreamDevice.h"
#include <QCheckBox>
#include <QTcpServer>

class DeviceWindow;

class DeviceWidget : public DeviceView
{
    Q_OBJECT
public:
    explicit DeviceWidget(DeviceConnection* connection, DeviceInfo* deviceInfo);
    ~DeviceWidget();

    void setupVideoConnection();
    void teardownVideoConnection();
    QByteArray grabFrame();

    DeviceWindow* getDeviceWindow() { return deviceWindow; }

    QCheckBox* const checkBox = new QCheckBox;

protected:
    bool event(QEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void launchDeviceWindow();

    DeviceWindow* deviceWindow = nullptr;

    LiveStreamDevice* m_videoDevice = nullptr;
    QTcpServer* m_videoServer = nullptr;
    DeviceConnection* m_usbVideoConnection = nullptr;
};
