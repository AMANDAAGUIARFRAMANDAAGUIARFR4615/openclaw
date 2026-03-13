#include "DeviceWindow.h"
#include "DeviceWidget.h"
#include "EventHub.h"
#include "UsbDeviceManager.h"
#include "MainWindow.h"
#include "VideoFrameWidget.h"
#include <QElapsedTimer>
#include <QVBoxLayout>
#include <QMimeData>
#include <QHBoxLayout>
#include <QPushButton>
#include <QOperatingSystemVersion>
#include <QApplication>
#include <QSlider>
#include <QImageReader>
#include <QScreen>
#include <QWindow>

#ifdef Q_OS_WIN
#include <winsock2.h>
#include <windows.h>
#include <winuser.h>
#endif

DeviceWindow::DeviceWindow(DeviceConnection* connection, DeviceInfo* deviceInfo, DeviceWidget* deviceWidget) : DeviceView(connection, deviceInfo), deviceWidget(deviceWidget)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(QString("%1%2%3").arg(deviceInfo->deviceName, deviceInfo->controller ? " 🏹主控" : "", deviceWidget->checkBox->isChecked() ? " 🎯被控" : ""));
    
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(overlay);
    setLayout(layout);

    addContextMenuActions();

    EventHub::on(this, "lockedStatus", [this](const QJsonValue &data, DeviceConnection* connection) {
        if (this->connection != connection)
            return;

        auto locked = data.toBool();

        if (locked)
            showOverlay("设备已锁定");
        else
            hideOverlay();
    });

    EventHub::on(this, "orientation", [this](const QJsonValue &data, DeviceConnection* connection) {
        if (this->connection != connection)
            return;

        auto orientation = data.toInt();
        this->deviceInfo->orientation = orientation;
        changeOrientation(orientation);
    });
}

DeviceWindow::~DeviceWindow()
{
    EventHub::off(this, "lockedStatus");
    EventHub::off(this, "audioPort");

    UsbDeviceManager::getInstance()->disconnectDevice(audioDeviceConnection);
    
    if (audioPlayer) {
        audioPlayer->stop();
        delete audioPlayer->sourceDevice();
        delete audioPlayer->audioOutput();
        delete audioPlayer;
    }
}

void DeviceWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
        close();
    else
        DeviceView::keyPressEvent(event);
}

void DeviceWindow::showEvent(QShowEvent *event)
{
    DeviceView::showEvent(event);
    changeOrientation(deviceInfo->orientation);
}

void DeviceWindow::changeOrientation(int orientation)
{
    auto width = size().width();
    auto height = size().height();

    if ((orientation == 1 || orientation == 2) && height < width || (orientation == 3 || orientation == 4) && height > width)
    {
        std::swap(width, height);
    }

#ifdef Q_OS_WIN
    this->layout()->setSizeConstraint(QLayout::SetFixedSize);
#endif
    setFixedSize(width, height);

    QTimer::singleShot(0, [=]() {
#ifdef Q_OS_WIN
        this->layout()->setSizeConstraint(QLayout::SetNoConstraint);
#endif
        setMinimumSize(0, 0);
        setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        resize(width, height);
    });
}

bool DeviceWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    float ratio = 1.0f * deviceInfo->screenWidth / deviceInfo->screenHeight;

    if (deviceInfo->orientation == 3 || deviceInfo->orientation == 4)
        ratio = 1 / ratio;

#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG") {
        MSG *msg = static_cast<MSG *>(message);

        if (msg->message == WM_SIZING) {
            RECT *r = (RECT *)msg->lParam;
            WPARAM edge = msg->wParam;

            // --- 步骤 1: 计算窗口边框和标题栏的尺寸 ---
            HWND hwnd = (HWND)this->winId();
            DWORD style = GetWindowLong(hwnd, GWL_STYLE);
            DWORD exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);

            RECT borderRect = {0, 0, 0, 0};
            AdjustWindowRectEx(&borderRect, style, FALSE, exStyle);
            
            int borderW = borderRect.right - borderRect.left;
            int borderH = borderRect.bottom - borderRect.top;

            // --- 步骤 2: 获取当前尝试拖拽后的“内容区域”大小 ---
            int totalW = r->right - r->left;
            int totalH = r->bottom - r->top;

            int clientW = totalW - borderW;
            int clientH = totalH - borderH;

            // --- 步骤 3: 根据内容区域计算理想尺寸 ---
            if (edge == WMSZ_TOP || edge == WMSZ_BOTTOM) {
                clientW = static_cast<int>(clientH * ratio);
            } else {
                clientH = static_cast<int>(clientW / ratio);
            }
            
            // --- 步骤 4: 限制检查 (基于内容区域 ClientSize) ---
            // 获取当前屏幕的可用区域（减去任务栏等）
            QScreen *screen = windowHandle() ? windowHandle()->screen() : qApp->primaryScreen();
            QRect screenRect = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);

            // 获取当前窗口的 DPI 缩放比例
            qreal dpr = devicePixelRatioF();

            // 将 Qt 的逻辑像素转换为 Windows 需要的物理像素
            int maxPhysicalWidth = static_cast<int>(this->maximumWidth() * dpr);
            int maxPhysicalHeight = static_cast<int>(this->maximumHeight() * dpr);
            int screenPhysicalWidth = static_cast<int>(screenRect.width() * dpr);
            int screenPhysicalHeight = static_cast<int>(screenRect.height() * dpr);

            // 计算允许的最大内容宽高
            int maxClientW = qMin(maxPhysicalWidth, screenPhysicalWidth - borderW);
            int maxClientH = qMin(maxPhysicalHeight, screenPhysicalHeight - borderH);

            // 进行限制判定
            if (clientW > maxClientW) {
                clientW = maxClientW;
                clientH = static_cast<int>(clientW / ratio);
            }
            if (clientH > maxClientH) {
                clientH = maxClientH;
                clientW = static_cast<int>(clientH * ratio);
            }
            // 二次检查 (防止因比例计算导致其中一边再次超标)
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
