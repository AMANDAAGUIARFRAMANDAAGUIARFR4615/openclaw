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
    QPoint getTransformedPosition(QPoint pos);
    void closeEvent(QCloseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    bool event(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

    Qt::MouseButtons pressedButtons = Qt::NoButton;
    QTimer *wheelTimer = nullptr;
    QPoint currentPos;
    int accumulatedDelta = 0;
    const int maxSteps = 5;
    int stepCount = 0;
    QMediaPlayer* audioPlayer = nullptr;
    DeviceConnection* audioDeviceConnection = nullptr;
};
