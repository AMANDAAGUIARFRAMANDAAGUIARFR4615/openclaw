#pragma once

#include "DeviceView.h"
#include "LiveStreamDevice.h"
#include <QCheckBox>
#include <QTcpServer>
#include <memory>

class DeviceWindow;
class ScreenStreamRecorder;

class DeviceWidget : public DeviceView
{
    Q_OBJECT
public:
    explicit DeviceWidget(DeviceConnection* connection, DeviceInfo* deviceInfo);
    ~DeviceWidget();

    void setupVideoConnection();
    void teardownVideoConnection();
    QByteArray grabFrame();

    void setStreamRecording(bool on);
    bool isStreamRecording() const { return static_cast<bool>(streamRecorder_); }

    DeviceWindow* getDeviceWindow() { return deviceWindow; }
    const DeviceWindow* getDeviceWindow() const { return deviceWindow; }

    QSize videoAreaChromeSize() const;
    void applyVideoAreaSize(const QSize &videoSize);

    QCheckBox* const checkBox = new QCheckBox;

protected:
    bool event(QEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void launchDeviceWindow();

    DeviceWindow* deviceWindow = nullptr;

    LiveStreamDevice* m_videoDevice = nullptr;
    QTcpServer* m_videoServer = nullptr;
    DeviceConnection* m_usbVideoConnection = nullptr;

    std::unique_ptr<ScreenStreamRecorder> streamRecorder_;
};
