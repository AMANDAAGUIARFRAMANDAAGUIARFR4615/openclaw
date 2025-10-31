#pragma once

#include "DeviceView.h"
#include <QResizeEvent>

class DeviceWidget;

class DeviceWindow : public DeviceView
{
    Q_OBJECT
public:
    explicit DeviceWindow(DeviceConnection* connection, DeviceInfo* deviceInfo, DeviceWidget* deviceWidget);
    ~DeviceWindow();

    DeviceWidget* const deviceWidget;

protected:
    QPointF getTransformedPosition(QMouseEvent *event);
    void closeEvent(QCloseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    bool event(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

    void resizeEvent(QResizeEvent *event) override
    {
        auto deviceInfo = connection->deviceInfo;
        double aspectRatio = 1.0 * deviceInfo->screenHeight / deviceInfo->screenWidth;

        resize(videoFrameWidget->size().width() + buttonContainer->size().width(), videoFrameWidget->size().width() * aspectRatio);
        setFixedHeight(videoFrameWidget->size().width() * aspectRatio);
     
        // resize(videoFrameWidget->size().height() / aspectRatio + buttonContainer->size().width(), videoFrameWidget->size().height());
        // setFixedWidth(videoFrameWidget->size().height() / aspectRatio + buttonContainer->size().width());
    }

    QWidget *buttonContainer;
    QTimer *wheelTimer = nullptr;
    QPoint currentPos;
    int accumulatedDelta = 0;
    const int maxSteps = 5;
    int stepCount = 0;
};
