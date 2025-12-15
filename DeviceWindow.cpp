#include "DeviceWindow.h"
#include "DeviceWidget.h"
#include "Logger.h"
#include "EventHub.h"
#include "ToastWidget.h"
#include "Tools.h"
#include "LiveStreamDevice.h"
#include "UsbDeviceManager.h"
#include "EmojiIconProvider.h"
#include "MainWindow.h"
#include <QElapsedTimer>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QMimeData>
#include <QHBoxLayout>
#include <QPushButton>
#include <QOperatingSystemVersion>
#include <QApplication>
#include <QSlider>
#include <QImageReader>

DeviceWindow::DeviceWindow(DeviceConnection* connection, DeviceInfo* deviceInfo) : DeviceView(connection, deviceInfo)
{
    setWindowTitle(connection->displayName());

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    setLayout(layout);

    EventHub::on(this, "lockedStatus", [this](const QJsonValue &data, DeviceConnection* connection) {
        if (this->connection != connection)
            return;

        auto locked = data.toBool();

        if (locked)
            showOverlay("设备已锁定");
        else
            hideOverlay();
    });
}

DeviceWindow::~DeviceWindow()
{
    EventHub::off(this, "lockedStatus");
    EventHub::off(this, "audioPort");

    g_usbDeviceManager->disconnectDevice(audioDeviceConnection);
    
    if (audioPlayer) {
        audioPlayer->stop();
        delete audioPlayer->sourceDevice();
        delete audioPlayer->audioOutput();
        delete audioPlayer;
    }
}

void DeviceWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    DeviceView::mouseDoubleClickEvent(event);

    qDebugEx() << "双击" << event->button();

    if (event->button() == Qt::LeftButton) {
        QMouseEvent *pressEvent = new QMouseEvent(QEvent::MouseButtonPress, event->pos(), event->button(), event->button(), event->modifiers());

        QMouseEvent *releaseEvent = new QMouseEvent(QEvent::MouseButtonRelease, event->pos(), event->button(), event->button(), event->modifiers());

        QApplication::postEvent(this, pressEvent);

        QTimer::singleShot(100, [=]() {
            QApplication::postEvent(this, releaseEvent);
        });
    }
}

void DeviceWindow::resizeEvent(QResizeEvent *event)
{
    if (videoFrameWidget) {
        overlay->move(videoFrameWidget->pos());
        overlay->resize(videoFrameWidget->size());
    }

    DeviceView::resizeEvent(event);
}

QMenu* DeviceWindow::createContextMenu()
{
    auto menu = DeviceView::createContextMenu();
    auto subMenu = menu->addMenu(EmojiIconProvider::createIcon("🎬"), "清晰度");
    subMenu->addAction("360p", [this]() {
        connection->send("setVideoQuality", 3);
    });
    subMenu->addAction("480p", [this]() {
        connection->send("setVideoQuality", 4);
    });
    subMenu->addAction("720p", [this]() {
        connection->send("setVideoQuality", 5);
    });
    return menu;
}
