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

#ifdef Q_OS_WIN
#include <windows.h>
#include <winuser.h>
#endif

DeviceWindow::DeviceWindow(DeviceConnection* connection, DeviceInfo* deviceInfo) : DeviceView(connection, deviceInfo)
{
    setWindowTitle(connection->displayName());

    QHBoxLayout *layout = new QHBoxLayout(this);
    // layout->setSizeConstraint(QLayout::SetFixedSize);
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

bool DeviceWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    float ratio = 1.0f * deviceInfo->screenWidth / deviceInfo->screenHeight;

#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG") {
        MSG *msg = static_cast<MSG *>(message);

        if (msg->message == WM_SIZING) {
            RECT *r = (RECT *)msg->lParam;
            WPARAM edge = msg->wParam;

            // --- 步骤 1: 计算窗口边框和标题栏的尺寸 ---
            // 获取窗口当前的样式
            HWND hwnd = (HWND)this->winId();
            DWORD style = GetWindowLong(hwnd, GWL_STYLE);
            DWORD exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);

            // 利用 API 计算只有边框时的大小（即内容为 0x0 时窗口多大）
            RECT borderRect = {0, 0, 0, 0};
            AdjustWindowRectEx(&borderRect, style, FALSE, exStyle);
            
            // borderW 是左右边框总和，borderH 是标题栏+底边框总和
            int borderW = borderRect.right - borderRect.left;
            int borderH = borderRect.bottom - borderRect.top;

            // --- 步骤 2: 获取当前尝试拖拽后的“内容区域”大小 ---
            int totalW = r->right - r->left;
            int totalH = r->bottom - r->top;

            int clientW = totalW - borderW;
            int clientH = totalH - borderH;

            // --- 步骤 3: 根据内容区域计算理想尺寸 ---
            
            // 逻辑：如果是拖动上下边，以高度为基准算宽度；否则优先以宽度算高度
            if (edge == WMSZ_TOP || edge == WMSZ_BOTTOM) {
                // 以高算宽
                clientW = static_cast<int>(clientH * ratio);
            } else {
                // 以宽算高 (默认)
                clientH = static_cast<int>(clientW / ratio);
            }
            
            // --- 步骤 4: 限制检查 (基于内容区域 ClientSize) ---
            // 注意：Qt 的 maximumWidth/Height 指的是 Client Size，不是 Window Size
            int maxClientW = this->maximumWidth();
            int maxClientH = this->maximumHeight();

            if (clientW > maxClientW) {
                clientW = maxClientW;
                clientH = static_cast<int>(clientW / ratio);
            }
            if (clientH > maxClientH) {
                clientH = maxClientH;
                clientW = static_cast<int>(clientH * ratio);
            }
            // 二次检查
            if (clientW > maxClientW) {
                clientW = maxClientW;
                clientH = static_cast<int>(clientW / ratio);
            }

            // --- 步骤 5: 还原为窗口总大小 ---
            int finalTotalW = clientW + borderW;
            int finalTotalH = clientH + borderH;

            // --- 步骤 6: 应用回 RECT ---
            switch (edge) {
            case WMSZ_LEFT:
                r->left = r->right - finalTotalW;
                r->bottom = r->top + finalTotalH;
                break;
            case WMSZ_RIGHT:
                r->right = r->left + finalTotalW;
                r->bottom = r->top + finalTotalH;
                break;
            case WMSZ_TOP:
                r->top = r->bottom - finalTotalH;
                r->right = r->left + finalTotalW;
                break;
            case WMSZ_BOTTOM:
                r->bottom = r->top + finalTotalH;
                r->right = r->left + finalTotalW;
                break;
            case WMSZ_TOPLEFT:
                r->left = r->right - finalTotalW;
                r->top = r->bottom - finalTotalH;
                break;
            case WMSZ_TOPRIGHT:
                r->right = r->left + finalTotalW;
                r->top = r->bottom - finalTotalH;
                break;
            case WMSZ_BOTTOMLEFT:
                r->left = r->right - finalTotalW;
                r->bottom = r->top + finalTotalH;
                break;
            case WMSZ_BOTTOMRIGHT:
                r->right = r->left + finalTotalW;
                r->bottom = r->top + finalTotalH;
                break;
            }

            *result = TRUE;
            return true;
        }
    }
#endif

    return DeviceView::nativeEvent(eventType, message, result);
}
