#pragma once

#include "DeviceInfo.h"
#include "DeviceConnection.h"
#include <QWidget>
#include <QUrl>
#include <QMenu>

class QMediaPlayer;
class VideoFrameWidget;
class QLabel;
class QPushButton;

class DeviceView : public QWidget
{
    Q_OBJECT
public:
    explicit DeviceView(DeviceConnection* connection, DeviceInfo* deviceInfo, QWidget *parent = nullptr);
    ~DeviceView();

    void setSourceDevice(QIODevice *device, const QUrl &sourceUrl = QUrl());

    void addVideoFrameWidget(VideoFrameWidget* videoFrameWidget);
    VideoFrameWidget* getVideoFrameWidget() { return videoFrameWidget; }

    DeviceConnection* const connection;
    DeviceInfo* const deviceInfo;

protected:
    void showOverlay(const QString &text);
    void hideOverlay();
    void send(const DataGuard::StrObfuscator<>& event, const QJsonValue &jsonValue = QJsonValue());
    void addContextMenuActions();
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    
    QPoint getTransformedPosition(QPoint pos);
    bool event(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

    VideoFrameWidget *videoFrameWidget = nullptr;
    QWidget *overlay = nullptr;
    QLabel *overlayLabel = nullptr;
    QPushButton *overlayUnlockButton = nullptr;

    Qt::MouseButtons pressedButtons = Qt::NoButton;
    QTimer *wheelTimer = nullptr;
    QPoint currentPos;
    int accumulatedDelta = 0;
    const int maxSteps = 5;
    int stepCount = 0;

    inline static QTimer *clipboardTimer = nullptr;
    inline static int clipboardTotal = 0;
    inline static int clipboardCount = 0;
};
