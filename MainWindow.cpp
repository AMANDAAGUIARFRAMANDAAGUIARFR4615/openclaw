#include "MainWindow.h"
#include "LogWindow.h"
#include "Tools.h"
#include "EmojiIconProvider.h"
#include "RemoteFileExplorer.h"
#include "EventHub.h"
#include "LiveStreamDevice.h"
#include "UsbDeviceManager.h"
#include "TcpServer.h"
#include "DeviceWidget.h"
#include "ToastWidget.h"
#include "SettingsViewer.h"
#include "FileTransfer.h"
#include "UdpTransport.h"
#include "AppSettingsDialog.h"
#include "JailbreakAssistantDialog.h"
#include <QShortcut>
#include <QVBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QApplication>
#include <QScreen>
#include <QApplication>
#include <QTabBar>
#include <QSplitter>
#include <QListWidgetItem>
#include <QListWidget>
#include <QStyle>
#include <QIcon>
#include <QTimer>
#include <cmath>
#include <QMenu>
#include <QDialog>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QFormLayout>
#include <QInputDialog>
#include <QFileDialog>
#include <QJsonObject>
#include <QToolButton>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setMinimumSize(800, 600);

    QSize screenSize = QApplication::primaryScreen()->size();
    resize(settings.value("mainWindowSize", screenSize * 0.8).toSize());

    int x = (screenSize.width() - width()) / 2;
    int y = (screenSize.height() - height()) / 2;
    move(x, y);

    auto central = new QWidget(this);
    setCentralWidget(central);

    auto mainLayout = new QHBoxLayout(central);

    auto splitter = new QSplitter(Qt::Horizontal, this);
    mainLayout->addWidget(splitter);
    splitter->setStyleSheet("QSplitter::handle {background-color: #B0B0B0;width: 1px;}");

    sideBarList = new QListWidget();
    sideBarList->setViewMode(QListView::IconMode);
    sideBarList->setFixedWidth(80);
    sideBarList->setStyleSheet("QListWidget::item { margin-top: 10px; margin-bottom: 10px; }");
    sideBarList->setCursor(Qt::PointingHandCursor);

    auto updateSideBar = [=]() {
        sideBarList->clear();

        auto sideBarMenu = AppSettingsDialog::getInstance()->getEnabledList("sideBarMenu");

        for (int i = 0; i < sideBarMenu.count(); i++) {
            auto text = sideBarMenu[i];
            QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, text);
            finder.toNextBoundary(); 
            int splitPos = finder.position();

            auto iconPart = text.left(splitPos);
            auto labelPart = text.mid(splitPos);

            auto item = new QListWidgetItem(EmojiIconProvider::createIcon(iconPart, 64, labelPart == "同屏操作" ? !isMultiControlEnabled : false), labelPart);
            sideBarList->addItem(item);
            item->setSizeHint(QSize(sideBarList->width() - 4, 70));
        }
    };

    updateSideBar();

    connect(AppSettingsDialog::getInstance(), &AppSettingsDialog::configurationChanged, [=](const QString &key) {
        if (key == "sideBarMenu")
        {
            updateSideBar();
            return;
        }

        auto value = AppSettingsDialog::getInstance()->getValue(key);

        if (key == "diaplayMode") {

            return;
        }

        if (key == "videoQuality") {
            return;
        }
    });

    connect(sideBarList, &QListWidget::itemClicked, [=](QListWidgetItem *item) {
        QString text = item->text();

        if (text == "设备连接") {
            auto qrDialog = new QDialog(this);
            qrDialog->setAttribute(Qt::WA_DeleteOnClose);
            qrDialog->setWindowTitle("扫码连接");

            auto mainLayout = new QHBoxLayout(qrDialog);
            mainLayout->setSizeConstraint(QLayout::SetFixedSize); 

            auto localIPs = NetworkUtils::getPhysicalIPs();
            qDebugEx() << "本机内网IP:" << localIPs;

            for (const QString &localIP : localIPs) {
                QJsonObject hostInfo = TcpServer::getInstance()->getHostInfo(localIP);
                QByteArray data = QJsonDocument(hostInfo).toJson().toBase64();
                
                int qrSize = qMax(200, 500 - (localIPs.size() * 100));

                auto img = Tools::generateQrImage(data);
                QPixmap pixmap = QPixmap::fromImage(img).scaled(
                    qrSize, qrSize, 
                    Qt::KeepAspectRatio, 
                    Qt::SmoothTransformation
                );

                QWidget *itemWidget = new QWidget(qrDialog);
                QVBoxLayout *itemLayout = new QVBoxLayout(itemWidget);

                QLabel *imgLabel = new QLabel(itemWidget);
                imgLabel->setPixmap(pixmap);
                imgLabel->setAlignment(Qt::AlignCenter);

                QLabel *textLabel = new QLabel(localIP, itemWidget);
                textLabel->setAlignment(Qt::AlignCenter);
                QFont font = textLabel->font();
                font.setPointSize(16);
                font.setBold(true);
                textLabel->setFont(font);

                itemLayout->addWidget(imgLabel);
                itemLayout->addWidget(textLabel);

                mainLayout->addWidget(itemWidget);
            }

            if (localIPs.isEmpty()) {
                QLabel *errLabel = new QLabel("未检测到有效网卡", qrDialog);
                mainLayout->addWidget(errLabel);
            }

            qrDialog->exec();
            return;
        }

        if (text == "分组群控") {
            auto send = [this](const QString& event, const QJsonValue &jsonValue = QJsonValue()) {
                auto bit = tabs[tabWidget->currentIndex()].bit;
                auto devices = DeviceInfo::getDevices(bit == 0 ? 0 : (1U << bit));

                for (const auto& device : devices) {
                    device->connection->send(event, jsonValue);
                }
            };

            QMenu menu(this);
            menu.addAction(EmojiIconProvider::createIcon("🔒"), "锁屏", [=]() {
                send("changeScreenLockedStatus", 1);
            });
            menu.addAction(EmojiIconProvider::createIcon("🔓"), "解锁", [=]() {
                send("changeScreenLockedStatus", 0);
            });
            menu.addAction(EmojiIconProvider::createIcon("🔄"), "重启", [=]() {
                send("reboot");
            });
            menu.addAction(EmojiIconProvider::createIcon("🔇"), "静音", [=]() {
                send("volumeControl", "OFF");
            });
            menu.addAction(EmojiIconProvider::createIcon("🔊"), "取消静音", [=]() {
                send("volumeControl", "ON");
            });
            menu.addAction(EmojiIconProvider::createIcon("📲"), "安装应用", [=]() {
                QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);

                QStringList files = QFileDialog::getOpenFileNames(
                    this,
                    "请选择 .ipa 或 .deb 文件",
                    desktopPath,
                    "*.ipa *.deb"
                );

                for (const auto &localPath : files) {
                    auto type = 2; // 收是1，发是2
                    auto size = Tools::getFileSize(localPath);

                    auto bit = tabs[tabWidget->currentIndex()].bit;
                    auto devices = DeviceInfo::getDevices(bit == 0 ? 0 : (1U << bit));

                    for (const auto& device : devices) {
                        auto transfer = new FileTransfer(device->connection, type, localPath, size);

                        QJsonObject dataObject;
                        dataObject["id"] = transfer->id;
                        dataObject["type"] = type;
                        dataObject["port"] = transfer->serverPort();
                        dataObject["name"] = localPath.section('/', -1);
                        dataObject["size"] = size;

                        device->connection->send("transferFile", dataObject);
                    }
                }
            });
            auto subMenu = menu.addMenu(EmojiIconProvider::createIcon("🎬"), "投屏清晰度");
            subMenu->addAction("360p", [=]() {
                send("setVideoQuality", 3);
            });
            subMenu->addAction("480p", [=]() {
                send("setVideoQuality", 4);
            });
            subMenu->addAction("720p", [=]() {
                send("setVideoQuality", 5);
            });
            menu.addAction(EmojiIconProvider::createIcon("📸"), "屏幕截图", [=]() {
                
            });
            menu.addAction(EmojiIconProvider::createIcon("🎥"), "屏幕录制")->setEnabled(false);
            QRect rect = sideBarList->visualItemRect(item);
            QPoint globalPos = sideBarList->viewport()->mapToGlobal(rect.topRight());
            menu.exec(globalPos);
            return;
        }

        if (text == "同屏操作") {
            isMultiControlEnabled = !isMultiControlEnabled;
            QIcon icon = EmojiIconProvider::createIcon("🕹️", 64, !isMultiControlEnabled);
            item->setIcon(icon);
            new ToastWidget(isMultiControlEnabled ? "同屏操作已开启" : "同屏操作已关闭", this);
            return;
        }

        if (text == "设置") {
            auto appSettingsDialog = AppSettingsDialog::getInstance();
            appSettingsDialog->setParent(this);
            appSettingsDialog->setWindowFlags(Qt::Dialog);
            appSettingsDialog->exec();
            return;
        }

        if (text == "帮助") {
            QDialog *helpDialog = new QDialog(this);
            helpDialog->setAttribute(Qt::WA_DeleteOnClose);
            helpDialog->setWindowTitle("帮助");
            helpDialog->resize(600, 500);

            QVBoxLayout *layout = new QVBoxLayout(helpDialog);

            QTextBrowser *textBrowser = new QTextBrowser(helpDialog);
            
            QString helpContent = R"(
                <style>
                    h3 { color: #333; }
                    h4 { color: #2E86C1; margin-bottom: 5px; }
                    li { margin-bottom: 5px; }
                    b { color: #444; }
                </style>

                <h3>📱 设备投屏与控制使用指南</h3>
                <hr>

                <h4>🔌 连接与显示</h4>
                <ul>
                    <li><b>连接方式：</b>支持 USB 有线及 Wi-Fi 无线连接，支持多设备同时控制。</li>
                    <li><b>设备分组：</b>支持创建自定义设备分组，便于分类管理大量设备。</li>
                    <li><b>灵活归属：</b><b>一个设备可同时属于多个分组</b>，极大提高设备管理的灵活性。</li>
                </ul>

                <h4>⌨️ 输入与多语言</h4>
                <ul>
                    <li><b>多国语言输入：</b>支持通过电脑键盘输入各种语言文字。</li>
                    <li><b>切换技巧：</b>需配合手机端键盘状态，使用 <b>Ctrl + Space</b> 可快捷切换手机输入法。</li>
                </ul>

                <h4>🖱️ 常用操作</h4>
                <ul>
                    <li><b>智能剪贴板（双向互通）：</b>
                        <br> - <b>Ctrl + C / V：</b> 在电脑和手机间<b>双向复制粘贴</b>（高效支持文本和图片）。
                        <br> - <b>Ctrl + Z / Y：</b> 支持文本编辑操作中的<b>撤销与重做</b>。
                    </li>
                    <li><b>基础交互：</b>鼠标左键实现点击、滑动操作；滚轮用于屏幕内容的快速翻页。</li>
                    <li><b>键盘导航：</b>方向键用于光标移动；使用 <b>Shift + 方向键</b> 可快速选中目标文本。</li>
                    <li><b>文件管理：</b>支持文件<b>重命名、删除、新建文件夹</b>等操作，以及 Zip/Rar 文件的<b>压缩与解压</b>。</li>
                    <li><b>多媒体：</b>电脑图片/视频导入手机相册；截图直接粘贴；脚本录制回放。</li>
                </ul>
            )";

            textBrowser->setHtml(helpContent);
            textBrowser->setOpenExternalLinks(true);

            layout->addWidget(textBrowser);

            QTimer::singleShot(0, [=](){
                auto docHeight = textBrowser->document()->size().height();
                helpDialog->resize(helpDialog->width(), docHeight + 30);
            });

            helpDialog->setModal(true);
            helpDialog->exec();
            return;
        }

        if (text == "越狱助手") {
            JailbreakAssistantDialog dialog(this);
            return;
        }

        if (text == "客服") {
            auto supportDialog = new QDialog(this);
            supportDialog->setAttribute(Qt::WA_DeleteOnClose);
            supportDialog->setWindowTitle("客服");

            auto mainLayout = new QHBoxLayout(supportDialog);
            mainLayout->setSizeConstraint(QLayout::SetFixedSize);

            auto pixmap = QPixmap(":/icons/support.jpg").scaled(
                640, 640,
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation
            );

            QLabel *imgLabel = new QLabel(supportDialog);
            imgLabel->setPixmap(pixmap);
            imgLabel->setAlignment(Qt::AlignCenter);

            mainLayout->addWidget(imgLabel);

            supportDialog->exec();
            return;
        }

        if (text == "日志") {
            auto logWindow = LogWindow::getInstance();
            logWindow->setParent(deviceListWidget);
            logWindow->resize(deviceListWidget->size());
            logWindow->toggleVisibility();
            return;
        }

        if (text == "开发者") {
            SettingsViewer dialog(&settings, this);
            return;
        }
    });

    splitter->addWidget(sideBarList);

    auto rightContainer = new QWidget(this);
    auto rightLayout = new QVBoxLayout(rightContainer);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    tabWidget = new QTabWidget(this);
    auto tabBar = tabWidget->tabBar();
    tabBar->setMovable(true);
    tabBar->setToolTip("右键点击可修改分组");
    tabBar->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(tabBar, &QTabBar::tabMoved, this, &MainWindow::onTabMoved);
    connect(tabBar, &QWidget::customContextMenuRequested, this, &MainWindow::showTabBarContextMenu);

    rightLayout->addWidget(tabWidget);

    auto controlBar = new QWidget(this);
    auto controlLayout = new QHBoxLayout(controlBar);

    zoomSlider = new QSlider(Qt::Horizontal, controlBar);
    zoomSlider->setRange(50, 500);

    auto zoomOutBtn = new QToolButton(controlBar);
    zoomOutBtn->setIcon(EmojiIconProvider::createIcon("➖")); 
    zoomOutBtn->setToolTip("缩小");
    zoomOutBtn->setAutoRaise(true);
    zoomOutBtn->setCursor(Qt::PointingHandCursor);
    
    connect(zoomOutBtn, &QToolButton::clicked, [=]() {
        zoomSlider->setValue(zoomSlider->value() - 10);
    });

    auto zoomInBtn = new QToolButton(controlBar);
    zoomInBtn->setIcon(EmojiIconProvider::createIcon("➕"));
    zoomInBtn->setToolTip("放大");
    zoomInBtn->setAutoRaise(true);
    zoomInBtn->setCursor(Qt::PointingHandCursor);

    connect(zoomInBtn, &QToolButton::clicked, [=]() {
        zoomSlider->setValue(zoomSlider->value() + 10);
    });
    
    auto percentLabel = new QLabel("100%", controlBar);
    percentLabel->setFixedWidth(40);
    percentLabel->setAlignment(Qt::AlignCenter);

    connect(zoomSlider, &QSlider::valueChanged, this, [=](int value) {
        tabs[tabWidget->currentIndex()].scale = value;
        percentLabel->setText(QString::number(value) + "%");
        relayoutDevices();
    });

    controlLayout->addWidget(zoomOutBtn);
    controlLayout->addWidget(zoomSlider);
    controlLayout->addWidget(zoomInBtn);
    controlLayout->addWidget(percentLabel);

    rightLayout->addWidget(controlBar);

    splitter->addWidget(rightContainer);

    deviceListWidget = new QListWidget(this);
    deviceListWidget->setViewMode(QListWidget::IconMode); // 图标模式（网格）
    deviceListWidget->setResizeMode(QListWidget::Adjust); // 随窗口自动调整换行
    deviceListWidget->setDragDropMode(QListWidget::NoDragDrop);
    deviceListWidget->setSpacing(10);
    deviceListWidget->setSortingEnabled(true);
    QLocale chineseLocale(QLocale::Chinese, QLocale::AnyCountry);
    deviceListWidget->setLocale(chineseLocale);
    deviceListWidget->sortItems(Qt::AscendingOrder);

    connect(tabBar, &QTabBar::currentChanged, this, [=](int index) {
        qDebugEx() << "onTabChanged" << index;

        auto widget = tabWidget->currentWidget();
        auto layout = widget->layout();
        if (!layout) {
            layout = new QVBoxLayout(widget);
            layout->setContentsMargins(0, 0, 0, 0);
        }
        
        if (deviceListWidget->parent() != widget) {
            layout->addWidget(deviceListWidget);
        }

        zoomSlider->setValue(tabs[index].scale);

        relayoutDevices();
    });

    EventHub::on(this, "deviceInfo", [this](const QJsonValue &data, DeviceConnection* connection) {
        connection->deviceInfo = new DeviceInfo(connection, data.toObject());
        addItem(connection);
    });

    EventHub::on(this, "disconnected", [this](const QJsonValue &data, DeviceConnection* connection) {
        qDebugEx() << "断开连接处理" << connection;
        if (!connection)
            return;

        for (int i = 0; i < deviceListWidget->count(); i++) {
            QListWidgetItem* item = deviceListWidget->item(i);
            if (item->data(Qt::UserRole).value<quintptr>() == (quintptr)connection->deviceInfo) {
                delete deviceListWidget->takeItem(i);
                break;
            }
        }
        
        relayoutDevices();
    });

    loadTabs();

    auto localIPs = NetworkUtils::getPhysicalIPs();
    qDebugEx() << "本机内网IP:" << localIPs;

    auto udpTransport = new UdpTransport(
        [](const QJsonObject &jsonObject) {
            qDebugEx() << "Received Data:" << jsonObject;
        }
    );

    auto broadcastTask = [=]() {
        const auto& ips = TcpServer::getInstance()->getConnectedIps();
        for (const auto &localIP : localIPs) {
            QList<QHostAddress> subnetIPs = NetworkUtils::getSubnetIPs(localIP);
            for (const QHostAddress &ip : subnetIPs) {
                if (!ips.contains(ip.toString()))
                    udpTransport->sendData(TcpServer::getInstance()->getHostInfo(localIP), ip, 32838);
            }
        }
    };

    broadcastTask();

    QTimer *timer = new QTimer(this);
    timer->callOnTimeout(broadcastTask);
    timer->start(3000);
}

MainWindow::~MainWindow()
{
    EventHub::off(this, "deviceInfo");
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!this->isMinimized())
        settings.setValue("mainWindowSize", normalGeometry().size());

    saveTabs();
    QApplication::quit();
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);

    if (event->type() == QEvent::WindowStateChange) {
        if (this->windowState() & Qt::WindowFullScreen) {
            sideBarList->hide();
            tabWidget->tabBar()->hide();
            zoomSlider->parentWidget()->hide();
        }
        else {
            sideBarList->show();
            tabWidget->tabBar()->show();
            zoomSlider->parentWidget()->show();
        }
        
        // 如果需要在切换时重新布局设备，可以在这里调用
        // relayoutDevices(); 
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (!isMultiControlEnabled || isDispatching)
        return false;

    auto sourceWidget = qobject_cast<DeviceWidget*>(watched);
    if (!sourceWidget)
        return false;

    QEvent::Type type = event->type();
    bool isMouseEvent = (type == QEvent::MouseButtonPress ||
                         type == QEvent::MouseButtonRelease ||
                         type == QEvent::MouseButtonDblClick ||
                         type == QEvent::MouseMove);
    bool isWheelEvent = (type == QEvent::Wheel);
    bool isKeyEvent = (type == QEvent::KeyPress ||
                       type == QEvent::KeyRelease);

    if (isMouseEvent || isWheelEvent || isKeyEvent) {
        isDispatching = true;

        for (int i = 0; i < deviceListWidget->count(); i++) {
            QListWidgetItem* item = deviceListWidget->item(i);
            
            if (item->isHidden()) continue;

            QWidget* widget = deviceListWidget->itemWidget(item);
            DeviceWidget* targetWidget = widget->findChild<DeviceWidget*>();

            if (targetWidget && targetWidget != sourceWidget) {
                if (isMouseEvent) {
                    QMouseEvent* me = static_cast<QMouseEvent*>(event);
                    QPointF localPos = me->localPos();
                    QPointF globalPos = targetWidget->mapToGlobal(localPos.toPoint());

                    QMouseEvent newEvent(type, localPos, me->windowPos(), globalPos,
                                         me->button(), me->buttons(), me->modifiers(), me->source());
                    QApplication::sendEvent(targetWidget, &newEvent);
                } 
                else if (isWheelEvent) {
                    QWheelEvent* we = static_cast<QWheelEvent*>(event);
                    QPointF globalPos = targetWidget->mapToGlobal(we->position().toPoint());
                    
                    QWheelEvent newEvent(we->position(), globalPos, we->pixelDelta(), we->angleDelta(),
                                         we->buttons(), we->modifiers(), we->phase(), we->inverted(), we->source());
                    QApplication::sendEvent(targetWidget, &newEvent);
                }
                else if (isKeyEvent) {
                    QKeyEvent* ke = static_cast<QKeyEvent*>(event);
                    QKeyEvent newEvent(type, ke->key(), ke->modifiers(), 
                                       ke->nativeScanCode(), ke->nativeVirtualKey(), ke->nativeModifiers(), 
                                       ke->text(), ke->isAutoRepeat(), ke->count());
                    QApplication::sendEvent(targetWidget, &newEvent);
                }
            }
        }

        isDispatching = false;
    }

    return false;
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    QMainWindow::keyPressEvent(event);

    if (event->matches(QKeySequence::FullScreen))
    {
        isFullScreen() ? showNormal() : showFullScreen();
        return;
    }

    if (event->key() == Qt::Key_Escape) {
        if (isFullScreen())
            showNormal();
        else
            close();
    }
}

void MainWindow::onTabMoved(int fromIndex, int toIndex)
{
    qDebugEx() << "onTabMoved" << fromIndex << toIndex;
    
    if (fromIndex == toIndex)
        return;

    auto movedTab = tabs.takeAt(fromIndex);
    tabs.insert(toIndex, movedTab);

    saveTabs();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    relayoutDevices();
}

void MainWindow::relayoutDevices()
{
    auto& [bit, _, rawScale, isLandscape] = tabs[tabWidget->currentIndex()];
    
    const auto& devicesInGroup = DeviceInfo::getDevices(bit == 0 ? 0 : (1U << bit));
    
    const auto scale = rawScale == 0 ? 1 : rawScale / 100.0f;

    int targetW = (isLandscape ? frameItemHeight : frameItemWidth) * scale;
    int targetH = (isLandscape ? frameItemWidth : frameItemHeight) * scale;
    QSize targetSize(targetW, targetH);

    for (int i = 0; i < deviceListWidget->count(); ++i) {
        QListWidgetItem* item = deviceListWidget->item(i);
        QWidget* widget = deviceListWidget->itemWidget(item);
        
        auto infoPtr = (DeviceInfo*)item->data(Qt::UserRole).value<quintptr>();

        item->setHidden(!infoPtr || !devicesInGroup.contains(infoPtr));

        if (!item->isHidden() && widget) {
            item->setSizeHint(targetSize);
            widget->setFixedSize(targetSize);
        }
    }
}

void MainWindow::addItem(DeviceConnection* connection)
{
    auto deviceInfo = connection->deviceInfo;

    auto player = new DeviceWidget(connection, deviceInfo);
    player->installEventFilter(this);

    auto device = new LiveStreamDevice(nullptr, 0, this);

    if (connection->type == DeviceConnection::Usb)
    {
        auto ctx = g_usbDeviceManager->getContext(connection);
        g_usbDeviceManager->connectDevice(ctx->udid, deviceInfo->videoPort, [=](DeviceConnection* conn, const QByteArray& data){
            device->appendData(data);
        });

        player->setSourceDevice(device);
    }
    else
    {
        // auto device = new LiveStreamDevice(deviceInfo->localIp, deviceInfo->videoPort, this);
        auto server = new QTcpServer(this);
        connect(server, &QTcpServer::newConnection, this, [=]() {
            QTcpSocket *socket = server->nextPendingConnection();
            qDebug() << "Client connected:" << socket->peerAddress().toString();
            connect(socket, &QTcpSocket::readyRead, this, [=]() {
                QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
                device->appendData(socket->readAll());
            });
        });
        server->listen(QHostAddress::Any, 0);
        connection->send("videoPort", server->serverPort());
        player->setSourceDevice(device);

        connect(player, &QObject::destroyed, [=]() {
            delete server;
        });
    }

    connect(player, &QObject::destroyed, [=]() {
        delete device;
    });

    auto frame = new QFrame();
    frame->setFrameShape(QFrame::Box);
    auto frameLayout = new QVBoxLayout(frame);
    frameLayout->setContentsMargins(0, 0, 0, 0);
    frameLayout->addWidget(player);

    auto item = new QListWidgetItem(deviceListWidget);
    item->setText(deviceInfo->deviceName);
    item->setData(Qt::UserRole, QVariant::fromValue((quintptr)deviceInfo));
    item->setSizeHint(QSize(frameItemWidth, frameItemHeight)); 
    
    deviceListWidget->addItem(item);
    deviceListWidget->setItemWidget(item, frame);

    relayoutDevices();
}

void MainWindow::showTabBarContextMenu(const QPoint &pos)
{
    int index = tabWidget->tabBar()->tabAt(pos);
    if (index < 0)
        return;

    auto tabBarMenu = AppSettingsDialog::getInstance()->getEnabledList("tabBarMenu");
    if (tabBarMenu.count() == 0)
        return;

    auto& [bit, _, __, isLandscape] = tabs[index];

    QMenu menu(this);

    for (int i = 0; i < tabBarMenu.count(); i++) {
        auto text = tabBarMenu[i];
        
        if (text == "横竖屏切换") {
            menu.addAction(text, [=]() {
                tabs[tabWidget->currentIndex()].isLandscape = !isLandscape;
                saveTabs(tabWidget->currentIndex());
                relayoutDevices();
            });
        }
        else if (text == "重命名分组") {
            menu.addAction(text, [=]() {
                bool ok = false;
                QString currentText = tabWidget->tabText(index);
                QString newName = QInputDialog::getText(
                    this,
                    "重命名分组",
                    "请输入新的分组名称：",
                    QLineEdit::Normal,
                    currentText,
                    &ok
                );
                if (ok && !newName.trimmed().isEmpty()) {
                    tabWidget->setTabText(index, newName.trimmed());
                    tabs[index].name = newName.trimmed();
                }
            });
        }
        else if (text == "删除分组") {
            menu.addAction(text, [&]() {
                if (QMessageBox::question(this, "删除确认", "分组内所有设备将被移除，\n确定删除？") != QMessageBox::Yes)
                    return;

                auto page = tabWidget->widget(index);
                tabWidget->removeTab(index);
                delete page;
                tabs.remove(index);
                auto tab = tabs.takeAt(index);
                auto mask = 1U << bit;
                auto devices = DeviceInfo::getDevices(mask);
                for (auto& deviceInfo : devices) {
                    deviceInfo->groupMask &= ~mask;
                    settings.setValue(deviceInfo->deviceId + "/groupMask", deviceInfo->groupMask);
                }
            })->setEnabled(bit != 0);
        }
        else if (text == "添加分组") {
            menu.addAction(text, [&]() {
                int newId = findAvailableTabId();
                if (newId == -1) {
                    new ToastWidget("已达到最大分组数", this);
                    return;
                }

                bool ok = false;
                QString newName = QInputDialog::getText(
                    this,
                    "添加分组",
                    "请输入新分组名称：",
                    QLineEdit::Normal,
                    "新分组",
                    &ok
                );
                if (ok && !newName.trimmed().isEmpty()) {
                    tabs.insert(index + 1, BitMaskEditorDialog::Item({newId, newName.trimmed()}));
                    tabWidget->insertTab(index + 1, new QWidget(), newName.trimmed());
                }
            });
        }
    }

    menu.exec(tabWidget->tabBar()->mapToGlobal(pos));
}

void MainWindow::loadTabs()
{
    int size = settings.beginReadArray("tabs");
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        auto bit = settings.value("bit").toInt();
        auto name = settings.value("name").toString();
        auto scale = settings.value("scale").toInt();
        auto isLandscape = settings.value("isLandscape").toBool();
        tabs.append(BitMaskEditorDialog::Item({bit, name, scale, isLandscape}));
        tabWidget->addTab(new QWidget(), name);
    }
    settings.endArray();

    if (tabWidget->count() == 0) {
        auto name = "默认分组";
        tabs.append(BitMaskEditorDialog::Item({0, name}));
        tabWidget->addTab(new QWidget(), name);
    }
}

void MainWindow::saveTabs(int index)
{
    settings.beginWriteArray("tabs");

    int start = 0;
    int end = tabs.size();

    if (index != -1) {
        start = index;
        end = index + 1;
    }
    
    for (int i = start; i < end; i++) {
        auto& [bit, name, scale, isLandscape] = tabs[i];
        settings.setArrayIndex(i);
        settings.setValue("bit", bit);
        settings.setValue("name", name);
        settings.setValue("scale", scale);
        settings.setValue("isLandscape", isLandscape); 
    }

    settings.endArray();
}

int MainWindow::findAvailableTabId()
{
    QSet<int> usedIds;
    for (const auto &tab : tabs) {
        usedIds.insert(tab.bit);
    }

    for (int bit = 0; bit < 32; ++bit) {
        if (!usedIds.contains(bit)) {
            return bit;
        }
    }

    return -1;
}
