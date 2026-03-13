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
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void changeOrientation(int orientation);
    void showEvent(QShowEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

    QMediaPlayer* audioPlayer = nullptr;
    DeviceConnection* audioDeviceConnection = nullptr;

    QFrame *buttonPanel;
    void updatePanelPosition();
};
