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
#include <QResizeEvent>
#include <QMoveEvent>
#include <QScrollArea> 
#include <QFrame>
#include <QScroller>
#include <cmath>

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

#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
    // 【移动端】作为普通子控件悬浮
    buttonPanel = new QFrame(this);
    buttonPanel->setObjectName("FloatingPanel");
    
    QHBoxLayout *panelLayout = new QHBoxLayout(buttonPanel);
#else
    // 【桌面端】作为独立工具窗口悬浮
    buttonPanel = new QFrame(this, Qt::Tool | Qt::FramelessWindowHint);
    buttonPanel->setAttribute(Qt::WA_TranslucentBackground); 
    buttonPanel->setObjectName("FloatingPanel"); 
    
    QVBoxLayout *panelLayout = new QVBoxLayout(buttonPanel);
#endif

    panelLayout->setContentsMargins(0, 0, 0, 0);
    panelLayout->setSpacing(0);

    QScrollArea *scrollArea = new QScrollArea(buttonPanel);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidgetResizable(true);

#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
    // 【移动端】水平滚动
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }"
        "QWidget#ScrollContent { background: transparent; }"
        "QScrollBar:horizontal { height: 0px; margin: 0px; }"
    );
    
    // QScroller::grabGesture(scrollArea->viewport(), QScroller::LeftMouseButtonGesture);
#else
    // 【桌面端】垂直滚动
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); 
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);    
    scrollArea->setStyleSheet("QScrollArea { background: transparent; border: none; }");
#endif

    QWidget *scrollContent = new QWidget();
    scrollContent->setObjectName("ScrollContent");

#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
    // 【移动端】按钮横排
    QHBoxLayout* buttonLayout = new QHBoxLayout(scrollContent);
    buttonLayout->setContentsMargins(0, 0, 0, 0); 
    buttonLayout->setSpacing(1);// 留 1px 的 spacing 作为按钮分割线
#else
    // 【桌面端】按钮竖排
    QVBoxLayout* buttonLayout = new QVBoxLayout(scrollContent);
    buttonLayout->setContentsMargins(8, 8, 8, 8); 
    buttonLayout->setSpacing(8);
#endif

    buttonLayout->setObjectName("buttonLayout");

    scrollArea->setWidget(scrollContent);
    panelLayout->addWidget(scrollArea);

    // 设置全局样式
#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
    // 【移动端】去掉所有圆角 (border-radius: 0px)，做成沉浸式底栏块
    buttonPanel->setStyleSheet(
        "QFrame#FloatingPanel { background-color: #1E2228; border-radius: 0px; }"
        "QPushButton { color: white; background-color: #0D74CE; border: none; border-radius: 0px; font-size: 15px; font-weight: bold; padding: 0 16px; }"
        "QPushButton:hover { background-color: #158AE5; }"
        "QPushButton:pressed { background-color: #0A5A9E; }"
    );
#else
    // 【桌面端】样式：垂直滚动条
    buttonPanel->setStyleSheet(
        "QFrame#FloatingPanel { background-color: rgba(30, 34, 40, 220); border-radius: 8px; }"
        "QPushButton { color: white; background-color: #0D74CE; border: none; border-radius: 5px; font-size: 14px; font-weight: bold; }"
        "QPushButton:hover { background-color: #158AE5; }"
        "QPushButton:pressed { background-color: #0A5A9E; }"
        "QScrollBar:vertical { background: transparent; width: 6px; margin: 4px 0px 4px 0px; }"
        "QScrollBar::handle:vertical { background: rgba(255, 255, 255, 80); border-radius: 3px; min-height: 20px; }"
        "QScrollBar::handle:vertical:hover { background: rgba(255, 255, 255, 120); }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }"
    );
#endif

    addContextMenuActions();

    if (!AppSettingsDialog::getInstance()->getValue("hideStandaloneToolbar"))
        buttonPanel->show();

#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
    closeButton = new QPushButton("关闭", this);
    closeButton->setCursor(Qt::PointingHandCursor);
    closeButton->setFixedSize(64, 30);
    closeButton->setStyleSheet(
        "QPushButton { color: #FFFFFF; background-color: #111827; border: 2px solid #F9FAFB; border-radius: 15px; font-size: 13px; font-weight: 700; }"
        "QPushButton:hover { background-color: #1F2937; }"
        "QPushButton:pressed { background-color: #000000; }"
    );
    connect(closeButton, &QPushButton::clicked, this, &QWidget::close);
    closeButton->raise();
#endif

    EventHub::on(this, "lockedStatus", [this](const QJsonValue &data, DeviceConnection* connection) {
        if (this->connection != connection)
            return;

        auto locked = data.toBool();
        this->deviceInfo->lockedStatus = locked;

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
    EventHub::off(this, "orientation");
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
    updatePanelPosition();
    updateCloseButtonPosition();
}

void DeviceWindow::resizeEvent(QResizeEvent *event)
{
    DeviceView::resizeEvent(event);
    updatePanelPosition();
    updateCloseButtonPosition();
}

void DeviceWindow::moveEvent(QMoveEvent *event)
{
    DeviceView::moveEvent(event);
    updatePanelPosition();
    updateCloseButtonPosition();
}

void DeviceWindow::updatePanelPosition()
{
    if (isMinimized() || !isVisible())
        return;

#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
    // ================= 【移动端定位】直接贴着最底下铺满 =================
    int clientW = width();
    int clientH = height();

    auto scrollArea = buttonPanel->findChild<QScrollArea*>();
    int panelHeight = scrollArea->widget()->sizeHint().height();
    
    int panelWidth = clientW; // 宽度直接铺满
    int x = 0; 
    int y = clientH - panelHeight; // y 坐标严丝合缝卡在最底部

    buttonPanel->setGeometry(x, y, panelWidth, panelHeight);
    buttonPanel->raise();

#else
    // ================= 【桌面端定位】悬浮于窗口右侧外部 =================
    auto mainRect = frameGeometry();
    
    int panelWidth = 110;
    auto scrollArea = buttonPanel->findChild<QScrollArea*>();
    int contentHeight = scrollArea->widget()->sizeHint().height();
    
    int maxPanelHeight = qMax(100, mainRect.height() - 20); 
    int panelHeight = qMin(contentHeight, maxPanelHeight);
    
    buttonPanel->resize(panelWidth, panelHeight);
    
    int gap = 10; 
    int x = mainRect.right() + gap;
    int y = mainRect.center().y() - (panelHeight / 2);
    
    buttonPanel->move(x, y);
#endif
}

void DeviceWindow::updateCloseButtonPosition()
{
    if (!closeButton)
        return;

    const int margin = 2;
    int safeLeft = 0;
    int safeTop = 0;

    if (QWindow *win = windowHandle()) {
        const auto insets = win->safeAreaMargins();
        safeLeft = static_cast<int>(std::ceil(insets.left()));
        safeTop = static_cast<int>(std::ceil(insets.top()));
    }

    // 基于系统安全区定位，避免刘海/状态栏遮挡。
    closeButton->move(safeLeft + margin, safeTop + margin);
    closeButton->raise();
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
        updateCloseButtonPosition();
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
