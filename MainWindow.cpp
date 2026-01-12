#include "MainWindow.h"
#include "LogWindow.h"
#include "Tools.h"
#include "EmojiIconProvider.h"
#include "EventHub.h"
#include "LiveStreamDevice.h"
#include "UsbDeviceManager.h"
#include "TcpServer.h"
#include "DeviceWidget.h"
#include "SettingsViewer.h"
#include "UdpTransport.h"
#include "AppSettingsDialog.h"
#include "JailbreakAssistantDialog.h"
#include "RenewalDialog.h"
#include "Account.h"
#include "LoginWidget.h"
#include "AccountListDialog.h"
#include "DeviceWindow.h"
#include "ExplicitSelectionListWidget.h"
#include "NaturalSortListWidgetItem.h"
#include <QShortcut>
#include <QVBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QApplication>
#include <QScreen>
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
#include <QActionGroup>
#include <QToolTip>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), tabWidget(new QTabWidget(this)), multiControlSwitchButton(new SwitchButton("同屏操作")), lineDispatcherSwitchButton(new SwitchButton("文本逐行分发"))
{
    setMinimumSize(800, 600);

    QSize screenSize = qApp->primaryScreen()->availableSize();
    resize(settings->value("mainWindowSize", screenSize * 0.8).toSize());

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

            auto item = new QListWidgetItem(EmojiIconProvider::createIcon(iconPart), labelPart);
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

        if (key == "isLandscape") {
            relayoutDevices();
            return;
        }

        if (key == "videoFps" || key == "videoQuality") {
            syncVideoSettingsToDevices();
            return;
        }
    });

    connect(sideBarList, &QListWidget::itemClicked, [this](QListWidgetItem *item) {
        QString title = item->text();
        qDebugEx() << "sideBarList" << title;

        if (title == "设备连接") {
            auto qrDialog = new QDialog(this);
            qrDialog->setAttribute(Qt::WA_DeleteOnClose);
            qrDialog->setWindowTitle("用手机APP扫码连接");

            auto mainLayout = new QHBoxLayout(qrDialog);
            mainLayout->setSizeConstraint(QLayout::SetFixedSize); 

            auto localIPs = NetworkUtils::getPhysicalIPs();
            qInfoEx() << "本机内网IP:" << localIPs;

            for (const QString &localIP : std::as_const(localIPs)) {
                const auto& hostInfo = TcpServer::getInstance()->getHostInfo(localIP);
                const auto& data = QJsonDocument(hostInfo).toJson(QJsonDocument::Compact).toBase64();
                
                int qrSize = qMax(200, 500 - (localIPs.size() * 100));

                auto img = Tools::generateQrImage(data);
                QPixmap pixmap = QPixmap::fromImage(img).scaled(
                    qrSize, qrSize, 
                    Qt::KeepAspectRatio, 
                    Qt::SmoothTransformation
                );

                auto itemWidget = new QWidget(qrDialog);
                auto itemLayout = new QVBoxLayout(itemWidget);

                auto imgLabel = new QLabel(itemWidget);
                imgLabel->setPixmap(pixmap);
                imgLabel->setAlignment(Qt::AlignCenter);

                auto textLabel = new QLabel(localIP, itemWidget);
                textLabel->setAlignment(Qt::AlignCenter);
                auto font = textLabel->font();
                font.setPointSize(16);
                font.setBold(true);
                textLabel->setFont(font);

                itemLayout->addWidget(imgLabel);
                itemLayout->addWidget(textLabel);

                mainLayout->addWidget(itemWidget);
            }

            if (localIPs.isEmpty()) {
                auto errLabel = new QLabel("未检测到有效网卡", qrDialog);
                mainLayout->addWidget(errLabel);
            }

            qrDialog->exec();
            return;
        }

        if (title == "设置") {
            auto dialog = AppSettingsDialog::getInstance();
            dialog->setParent(this);
            dialog->setWindowFlags(Qt::Dialog);
            dialog->exec();
            return;
        }

        if (title == "帮助") {
            auto helpDialog = new QDialog(this);
            helpDialog->setAttribute(Qt::WA_DeleteOnClose);
            helpDialog->setWindowTitle(title);
            helpDialog->resize(600, 500);

            auto layout = new QVBoxLayout(helpDialog);

            auto textBrowser = new QTextBrowser(helpDialog);
            
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

        if (title == "越狱助手") {
            JailbreakAssistantDialog dialog(this);
            dialog.exec();
            return;
        }

        if (title == "手机软件源") {
            auto repoDialog = new QDialog(this);
            repoDialog->setAttribute(Qt::WA_DeleteOnClose);
            repoDialog->setWindowTitle("使用相机APP扫码");

            auto mainLayout = new QVBoxLayout(repoDialog);

            auto tabWidget = new QTabWidget(repoDialog);

            struct SourceInfo { QString title; QString url; };
            QList<SourceInfo> sources = {
                {"Sileo", "sileo://source/https://sileo-1302990626.cos.ap-guangzhou.myqcloud.com"},
                {"Cydia", "cydia://url/https://cydia.saurik.com/api/share#?source=https://cydia-1302990626.cos.ap-guangzhou.myqcloud.com"}
            };

            int qrSize = 400;

            for (const auto& source : sources) {
                auto page = new QWidget();
                auto vLayout = new QVBoxLayout(page);
                
                auto img = Tools::generateQrImage(source.url);
                QPixmap pixmap = QPixmap::fromImage(img).scaled(
                    qrSize, qrSize, 
                    Qt::KeepAspectRatio, 
                    Qt::SmoothTransformation
                );
                
                auto imgLabel = new QLabel(page);
                imgLabel->setPixmap(pixmap);
                imgLabel->setAlignment(Qt::AlignCenter);

                const auto& displayUrl = source.url.mid(source.url.lastIndexOf("https://"));
                
                QString richText = QString("如不方便扫码，请手动输入软件源地址：<br><a href=\"%1\" style=\"color: #0078d7; text-decoration: none;\">%1</a>")
                   .arg(displayUrl);
                auto textLabel = new QLabel(richText, page);
                textLabel->setAlignment(Qt::AlignCenter);
                textLabel->setWordWrap(true);

                textLabel->setTextInteractionFlags(Qt::TextBrowserInteraction); // 允许交互
                textLabel->setOpenExternalLinks(false); // 禁止自动打开浏览器，我们要自己处理点击

                connect(textLabel, &QLabel::linkActivated, [displayUrl](const QString &link) {
                    qApp->clipboard()->setText(link);
                    QToolTip::showText(QCursor::pos(), "地址已复制");
                });
                
                vLayout->addWidget(imgLabel);
                vLayout->addWidget(textLabel);
                
                tabWidget->addTab(page, source.title);
            }

            mainLayout->addWidget(tabWidget);
            repoDialog->exec();
            return;
        }

        if (title == "续费") {
            RenewalDialog dialog(this);
            dialog.exec();
            return;
        }

        if (title == "客服") {
            showSupportDialog();
            return;
        }

        if (title == "日志") {
            auto logWindow = LogWindow::getInstance();
            logWindow->setParent(deviceListWidget);
            logWindow->resize(deviceListWidget->size());
            logWindow->toggleVisibility();
            return;
        }

        if (title == "开发者") {
            QMenu menu(this);
            menu.addAction("数据查看", [=]() {
                SettingsViewer dialog(settings, this);
                dialog.exec();
            });
            menu.addAction("兑换码生成", [this]() {
                bool ok;
                int count = QInputDialog::getInt(this, "生成兑换码", "请输入生成的个数:", 1, 1, 1000, 1, &ok);
                
                if (!ok)
                    return;

                webSocketClient->emitEvent("generate_codes", count, [=](const QJsonValue &res) {
                    if (res.isString()) {
                        QToolTip::showText(QCursor::pos(), res.toString());
                        return;
                    }

                    const auto& array = res.toArray();
                    QStringList codes;

                    for (const QJsonValue &item : array) {
                        codes << item.toString(); 
                    }

                    qApp->clipboard()->setText(codes.join("\n"));

                    QToolTip::showText(QCursor::pos(), "兑换码已复制到剪切板");
                });
            });
            menu.addAction("在线用户", [this]() {
                webSocketClient->emitEvent("online_accounts", QJsonValue(), [=](const QJsonValue &res) {
                    if (res.isString()) {
                        QToolTip::showText(QCursor::pos(), res.toString());
                        return;
                    }

                    const auto& array = res.toArray();
                    QStringList phoneNumbers;
                    for (const QJsonValue &item : array) {
                        phoneNumbers << item.toString();
                    }

                    if (phoneNumbers.isEmpty()) {
                        QToolTip::showText(QCursor::pos(), "没有在线账号");
                        return;
                    }

                    AccountListDialog dialog(phoneNumbers, this);
                    dialog.exec();
                });
            });

            QRect rect = sideBarList->visualItemRect(item);
            QPoint globalPos = sideBarList->viewport()->mapToGlobal(rect.topRight());
            menu.exec(globalPos);
            return;
        }
    });

    splitter->addWidget(sideBarList);

    auto rightContainer = new QWidget(this);
    auto rightLayout = new QVBoxLayout(rightContainer);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(5);

    auto hLayout = new QHBoxLayout();
    hLayout->setContentsMargins(5, 5, 5, 5);
    hLayout->setSpacing(5);
    hLayout->addWidget(multiControlSwitchButton);
    hLayout->addWidget(lineDispatcherSwitchButton);
    hLayout->addStretch();
    rightLayout->addLayout(hLayout);

    auto tabBar = tabWidget->tabBar();
    tabBar->setMovable(true);
    tabBar->setToolTip("右键点击可修改分组");
    tabBar->setContextMenuPolicy(Qt::CustomContextMenu);
    QFont font = tabWidget->font();
    font.setPixelSize(16);
    tabWidget->setFont(font);

    connect(tabBar, &QTabBar::tabMoved, this, &MainWindow::onTabMoved);
    connect(tabBar, &QWidget::customContextMenuRequested, this, &MainWindow::showTabBarContextMenu);

    rightLayout->addWidget(tabWidget);

    auto controlBar = new QWidget(this);
    auto controlLayout = new QHBoxLayout(controlBar);

    zoomSlider = new QSlider(Qt::Horizontal, controlBar);
    zoomSlider->setRange(20, 500);

    auto zoomOutBtn = new QToolButton(controlBar);
    zoomOutBtn->setIcon(EmojiIconProvider::createIcon("➖")); 
    zoomOutBtn->setToolTip("缩小");
    zoomOutBtn->setAutoRaise(true);
    
    connect(zoomOutBtn, &QToolButton::clicked, [=]() {
        zoomSlider->setValue(zoomSlider->value() - 10);
    });

    auto zoomInBtn = new QToolButton(controlBar);
    zoomInBtn->setIcon(EmojiIconProvider::createIcon("➕"));
    zoomInBtn->setToolTip("放大");
    zoomInBtn->setAutoRaise(true);

    connect(zoomInBtn, &QToolButton::clicked, [=]() {
        zoomSlider->setValue(zoomSlider->value() + 10);
    });
    
    auto percentLabel = new QLabel("100%", controlBar);
    percentLabel->setFixedWidth(40);
    percentLabel->setAlignment(Qt::AlignCenter);

    connect(zoomSlider, &QSlider::valueChanged, [=](int value) {
        getTab().scale = value;
        percentLabel->setText(QString::number(value) + "%");
        relayoutDevices();
    });

    controlLayout->addWidget(zoomOutBtn);
    controlLayout->addWidget(zoomSlider);
    controlLayout->addWidget(zoomInBtn);
    controlLayout->addWidget(percentLabel);

    rightLayout->addWidget(controlBar);

    splitter->addWidget(rightContainer);

    deviceListWidget = new ExplicitSelectionListWidget(this);
    deviceListWidget->setViewMode(QListWidget::IconMode); // 图标模式（网格）
    deviceListWidget->setResizeMode(QListWidget::Adjust); // 随窗口自动调整换行
    deviceListWidget->setDragDropMode(QListWidget::NoDragDrop);
    deviceListWidget->setSpacing(10);
    deviceListWidget->setSortingEnabled(true);
    deviceListWidget->sortItems(Qt::AscendingOrder);
    deviceListWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(deviceListWidget, &QListWidget::itemPressed, [this](QListWidgetItem *item) {
        auto widget = deviceListWidget->itemWidget(item);
        widget->findChild<DeviceWidget*>()->setFocus();
    });
    connect(deviceListWidget->selectionModel(), &QItemSelectionModel::selectionChanged, this, [=](const QItemSelection &selected, const QItemSelection &deselected) {
        // 1. 处理新被选中的项 (Selected)
        for (const QModelIndex &index : selected.indexes()) {
            auto player = index.data(Qt::UserRole).value<DeviceWidget*>();
            if (player && !player->checkBox->isChecked()) {
                const QSignalBlocker blocker(player->checkBox);
                player->checkBox->setChecked(true);
            }
        }

        // 2. 处理被取消选中的项 (Deselected)
        for (const QModelIndex &index : deselected.indexes()) {
            auto player = index.data(Qt::UserRole).value<DeviceWidget*>();
            if (player && player->checkBox->isChecked()) {
                const QSignalBlocker blocker(player->checkBox);
                player->checkBox->setChecked(false);
            }
        }
    });

    videoVisibilityManager = new VideoVisibilityManager(deviceListWidget, this);

    connect(tabBar, &QTabBar::currentChanged, [=](int index) {
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
        auto deviceInfo = DeviceInfo::getDevice(data["deviceId"].toString());

        if (deviceInfo) {
            if (deviceInfo->connection->type == connection->type) {
                connection->send("reject", "已存在相同连接");
                connection->close();
                return;
            }

            bool isUsbDevice = deviceInfo->connection->type == DeviceConnection::Usb;
            bool isUsbSetting = MainWindow::getInstance()->getTab().getConnectionMethod() == 0;

            if (isUsbDevice == isUsbSetting)
            {
                connection->send("reject", "已存在相同连接");
                connection->close();
                return;
            }
            else
            {
                deviceInfo->connection->close();
            }
        }

        // for (int i = 0; i < 100; i++) {
            connection->deviceInfo = new DeviceInfo(connection, data.toObject());
            addItem(connection);
        // }
    });

    EventHub::on(this, "disconnected", [this](const QJsonValue &data, DeviceConnection* connection) {
        qDebugEx() << "断开连接处理" << connection;

        if (!connection)
            return;

        for (int i = 0; i < deviceListWidget->count(); i++) {
            QListWidgetItem* item = deviceListWidget->item(i);
            auto deviceWidget = item->data(Qt::UserRole).value<DeviceWidget*>();
            if (deviceWidget->connection == connection) {
                delete deviceListWidget->takeItem(i);
                break;
            }
        }
    });

    webSocketClient->on("online_devices", [this](const QJsonValue &data, AckCallback callback) {
        const auto& devices = getDevices();

        QJsonArray jsonArray;

        for (const auto& device : std::as_const(devices)) {
            QJsonObject jsonObject;
            
            jsonObject.insert("deviceId", device->deviceId);
            jsonObject.insert("deviceName", device->deviceName);
            jsonObject.insert("model", device->model);
            jsonObject.insert("lockedStatus", device->lockedStatus);
            
            jsonArray.append(jsonObject);
        }

        callback(jsonArray);
    });

    webSocketClient->on("get_log", [this](const QJsonValue &data, AckCallback callback) {
        QString logFilePath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/app_log.txt";
        
        QFile file(logFilePath);
        if (file.exists() && file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            callback(in.readAll());
            file.close();
        } else {
            callback("无法读取日志文件:" + logFilePath);
        }
    });

    webSocketClient->on("screenshot", [this](const QJsonValue &data, AckCallback callback) {
        const auto& udid = data.isString() ? data.toString() : nullptr;
        if (udid != nullptr && !udid.isEmpty())
        {
            for (int i = 0; i < deviceListWidget->count(); i++) {
                QListWidgetItem* item = deviceListWidget->item(i);
                auto deviceWidget = item->data(Qt::UserRole).value<DeviceWidget*>();
                if (deviceWidget->deviceInfo->deviceId == udid) {
                    const auto& byteArray = deviceWidget->grabFrame();
                    callback(QJsonValue::fromVariant(byteArray.toBase64()));
                    break;
                }
            }
        }
        else
        {
            QByteArray byteArray;
            QBuffer buffer(&byteArray);
            buffer.open(QIODevice::WriteOnly);

            grab().save(&buffer, "JPG");
            callback(QJsonValue::fromVariant(byteArray.toBase64()));
        }
    });

    loadTabs();

    auto localIPs = NetworkUtils::getPhysicalIPs();
    qInfoEx() << "本机内网IP:" << localIPs;

    auto udpTransport = new UdpTransport(0, this);

    auto broadcastTask = [=]() {
        if (getTab().getAutoScanLANDevices() == 0)
            return;

        const auto& ips = TcpServer::getInstance()->getConnectedIps();
        for (const auto &localIP : localIPs) {
            QList<QHostAddress> subnetIPs = NetworkUtils::getSubnetIPs(localIP);
            for (const QHostAddress &ip : std::as_const(subnetIPs)) {
                if (!ips.contains(ip.toString())) {
                    auto deviceInfo = DeviceInfo::getDevice(ip.toString());
                    bool isUsbSetting = getTab().getConnectionMethod() == 0;
                    if (!deviceInfo || !isUsbSetting)
                        udpTransport->sendData(TcpServer::getInstance()->getHostInfo(localIP), ip, 32838);
                }
            }
        }
    };

    bool isUsbSetting = getTab().getConnectionMethod() == 0;
    if (!isUsbSetting)
        broadcastTask();

    int* elapsed = new int(0);
    elapsedTimer->start();

    QTimer *timer = new QTimer(this);
    timer->callOnTimeout([=](){
        *elapsed += 3000;
#ifndef QT_DEBUG
        if (qAbs(elapsedTimer->elapsed() - *elapsed) > 60000)
        {
#ifdef Q_OS_WIN
            __fastfail(7);
#endif
            *(int*)qApp = 0;
        }
#endif

        broadcastTask();
    });
    timer->start(3000);
}

MainWindow::~MainWindow()
{
    EventHub::off(this, "deviceInfo");
}

void MainWindow::showSupportDialog()
{
    if (!QFile::exists(qApp->applicationDirPath() + "/support.jpg")) {
        QToolTip::showText(QCursor::pos(), "客服图片未正确配置");
        return;
    }

    auto supportDialog = new QDialog(this);
    supportDialog->setAttribute(Qt::WA_DeleteOnClose);
    supportDialog->setWindowTitle("客服");

    auto mainLayout = new QHBoxLayout(supportDialog);
    mainLayout->setSizeConstraint(QLayout::SetFixedSize);

    auto pixmap = QPixmap(qApp->applicationDirPath() + "/support.jpg").scaled(
        640, 640,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
    );

    auto imgLabel = new QLabel(supportDialog);
    imgLabel->setPixmap(pixmap);
    imgLabel->setAlignment(Qt::AlignCenter);

    mainLayout->addWidget(imgLabel);

    supportDialog->exec();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!this->isMinimized())
        settings->setValue("mainWindowSize", normalGeometry().size());

    saveTabs();
    qApp->quit();
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
    }
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

void MainWindow::addOptionMenu(QMenu* parent, const QString& title, const QStringList& items, std::optional<int>* targetVar, std::function<void()> onChanged)
{
    auto subMenu = parent->addMenu(title);
    auto actionGroup = new QActionGroup(subMenu);

    for (int i = 0; i < items.size(); i++) {
        QString text = items[i];
        if (text.isEmpty()) 
            continue;

        std::optional<int> itemValue = text == "默认" ? std::nullopt : std::make_optional(i - 1);

        auto action = subMenu->addAction(text, [=]() {
            *targetVar = itemValue;

            if (onChanged)
                onChanged();
        });

        action->setCheckable(true);
        actionGroup->addAction(action);

        if (*targetVar == itemValue)
            action->setChecked(true);
    }
}

void MainWindow::syncVideoSettingsToDevices()
{
    const auto& devices = getDeviceWidgets();
    for (const auto& device : devices) {
        auto videoFrameWidget = device->getDeviceWindow() ? device->getDeviceWindow()->getVideoFrameWidget() : device->getVideoFrameWidget();
        const auto& size = videoFrameWidget->size();
        qApp->postEvent(videoFrameWidget, new QResizeEvent(size, size));
    }
}

void MainWindow::relayoutDevices()
{
    auto& tab = getTab();
    const auto bit = tab.bit;
    const auto scale = tab.scale == 0 ? 1 : tab.scale / 100.0f;
    const auto isLandscape = tab.getIsLandscape();
    
    const auto& devicesInGroup = DeviceInfo::getDevices(bit == 0 ? 0 : (1U << bit));
    
    auto frameItemHeight = frameItemWidth * DeviceInfo::getOptimalAspectRatio(devicesInGroup);
    int targetW = (isLandscape ? frameItemHeight : frameItemWidth) * scale;
    int targetH = (isLandscape ? frameItemWidth : frameItemHeight) * scale;
    QSize targetSize(targetW, targetH);

    for (int i = 0; i < deviceListWidget->count(); ++i) {
        auto item = deviceListWidget->item(i);
        
        auto deviceWidget = item->data(Qt::UserRole).value<DeviceWidget*>();

        item->setHidden(!deviceWidget || !devicesInGroup.contains(deviceWidget->deviceInfo));

        if (!item->isHidden()) {
            item->setSizeHint(targetSize + QSize(0, 46));
            deviceWidget->setFixedSize(targetSize + QSize(0, 46));
        }
    }

    // videoVisibilityManager->refresh();
}

void MainWindow::addItem(DeviceConnection* connection)
{
    connection->send("server", QJsonObject{{"accountId", Account::getInstance()->id}, {"ip", Config::SERVER_IP}, {"port", Config::SERVER_PORT}});

    auto deviceInfo = connection->deviceInfo;

    auto player = new DeviceWidget(connection, deviceInfo);

    deviceInfo->expireAt = LoginWidget::expirations.value(deviceInfo->deviceId);
    if (deviceInfo->expireAt.get() == 0)
    {
        auto retryTimer = new QTimer(player);
        
        connect(retryTimer, &QTimer::timeout, player, [=]() {
            retryTimer->setInterval(3000);

            if (deviceInfo->expireAt.get() != 0) {
                retryTimer->deleteLater();
                return;
            }

            webSocketClient->emitEvent("deviceExpireAt", deviceInfo->deviceId, [=](const QJsonValue &res) {
                deviceInfo->expireAt = res.toInteger();
            });
        });

        retryTimer->start(0);
    }

    auto ipLabel = player->findChild<QLabel*>("ipLabel");

    auto device = new LiveStreamDevice(player);

    if (connection->type == DeviceConnection::Usb)
    {
        auto videoConnection = UsbDeviceManager::getInstance()->connectDevice(deviceInfo->deviceId, deviceInfo->videoPort, true);

        connect(UsbDeviceManager::getInstance(), &UsbDeviceManager::rawDataReceived, player, [=](DeviceConnection* sender, const QByteArray& data){
            if (sender != videoConnection)
                return;

            if (deviceInfo->expireAt.get() > Account::getInstance()->loginTime.get() + elapsedTimer->elapsed())
            {
                device->appendData(data);
                ipLabel->setText(deviceInfo->localIp);
            }
            else
            {
                ipLabel->setText(deviceInfo->localIp + (deviceInfo->expireAt.get() == 0 ? "" : "<font color='red'>[已过期]</font>"));
            }
        });

        player->setSourceDevice(device);
    }
    else
    {
        auto server = new QTcpServer(player);
        connect(server, &QTcpServer::newConnection, [=]() {
            QTcpSocket *socket = server->nextPendingConnection();
            qDebugEx() << "投屏连接" << socket->peerAddress().toString();
            connect(socket, &QTcpSocket::readyRead, [=]() {
                const auto& data = socket->readAll();

                if (deviceInfo->expireAt.get() > Account::getInstance()->loginTime.get() + elapsedTimer->elapsed())
                {
                    device->appendData(data);
                    ipLabel->setText(deviceInfo->localIp);
                }
                else
                {
                    ipLabel->setText(deviceInfo->localIp + (deviceInfo->expireAt.get() == 0 ? "" : "<font color='red'>[已过期]</font>"));
                }
            });
        });
        server->listen(QHostAddress::Any, 0);
        connection->send("videoPort", server->serverPort());
        player->setSourceDevice(device);
    }

    auto frame = new QFrame();
    frame->setFrameShape(QFrame::Box);
    auto frameLayout = new QVBoxLayout(frame);
    frameLayout->setContentsMargins(0, 0, 0, 0);
    frameLayout->addWidget(player);

    auto item = new NaturalSortListWidgetItem(deviceListWidget);
    item->setText(deviceInfo->deviceName);
    item->setData(Qt::UserRole, QVariant::fromValue(player));

    connect(player->checkBox, &QCheckBox::stateChanged, [=](int state) {
        const QSignalBlocker blocker(deviceListWidget);
        item->setSelected(state == Qt::Checked);
        QToolTip::showText(QCursor::pos(), "按住Ctrl键，用鼠标点击小窗口可快捷切换选中状态");
    });

    player->checkBox->setCheckState(Qt::CheckState::Checked);
    
    deviceListWidget->addItem(item);
    deviceListWidget->setItemWidget(item, frame);

    player->setProperty("listWidgetItem", QVariant::fromValue(item));

    relayoutDevices();
}

QList<DeviceInfo*> MainWindow::getDevices()
{
    QList<DeviceInfo*> devices;
 
    for (const auto& widget : getDeviceWidgets()) {
        devices.append(widget->deviceInfo);
    }

    return devices;
}

QList<DeviceWidget*> MainWindow::getDeviceWidgets()
{
    QList<DeviceWidget*> list;

    for (int i = 0; i < deviceListWidget->count(); i++) {
        auto item = deviceListWidget->item(i);
        if (item->isHidden())
            continue;

        auto deviceWidget = item->data(Qt::UserRole).value<DeviceWidget*>();
        if (deviceWidget->checkBox->isChecked())
            list.append(deviceWidget);
    }

    return list;
}

QList<DeviceWindow*> MainWindow::getDeviceWindows()
{
    QList<DeviceWindow*> devices;
 
    for (const auto& widget : getDeviceWidgets()) {
        if (widget->getDeviceWindow())
            devices.append(widget->getDeviceWindow());
    }

    return devices;
}

void MainWindow::showTabBarContextMenu(const QPoint &pos)
{
    int index = tabWidget->tabBar()->tabAt(pos);
    if (index < 0)
        return;

    auto tabBarMenu = AppSettingsDialog::getInstance()->getEnabledList("tabBarMenu");
    if (tabBarMenu.count() == 0)
        return;

    auto& [bit, _, __, isLandscape, videoFps, videoQuality, connectionMethod, autoScanLANDevices, autoConnectUSBDevices] = tabs[index];

    QMenu menu(this);

    for (int i = 0; i < tabBarMenu.count(); i++) {
        auto text = tabBarMenu[i];
        
        if (text == "重命名分组") {
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
                    saveTabs(index);
                }
            });
        }
        else if (text == "删除分组") {
            menu.addAction(text, [&]() {
                if (QMessageBox::question(this, "删除确认", "分组内所有设备将被移除，\n确定删除？") != QMessageBox::Yes)
                    return;

                auto page = tabWidget->widget(index);
                tabWidget->removeTab(index);
                page->deleteLater();
                auto tab = tabs.takeAt(index);
                auto mask = 1U << bit;
                auto devices = DeviceInfo::getDevices(mask);
                for (auto& deviceInfo : devices) {
                    deviceInfo->groupMask &= ~mask;
                    settings->setValue(deviceInfo->deviceId + "/groupMask", deviceInfo->groupMask);
                }
                saveTabs();
            })->setEnabled(bit != 0);
        }
        else if (text == "添加分组") {
            menu.addAction(text, [&]() {
                int newId = findAvailableTabId();
                if (newId == -1) {
                    QToolTip::showText(QCursor::pos(), "已达到最大分组数");
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
                    saveTabs();
                }
            });
        }
        else if (text == "投屏显示") {
            QStringList list = {"默认", "竖屏", "横屏"};
            addOptionMenu(&menu, text, list, &isLandscape, [=]() {
                saveTabs(index);

                if (tabWidget->currentIndex() == index)
                    relayoutDevices();
            });
        }
        else if (text == "视频帧率") {
            QStringList list = {"默认", "", "5秒1帧", "1秒1帧", "1秒15帧", "1秒30帧"};
            addOptionMenu(&menu, text, list, &videoFps, [=]() {
                saveTabs(index);

                if (tabWidget->currentIndex() == index)
                    syncVideoSettingsToDevices();
            });
        }
        else if (text == "视频清晰度") {
            QStringList list = {"默认", "", "图片流", "标清", "高清", "超清"};
            addOptionMenu(&menu, text, list, &videoQuality, [=]() {
                saveTabs(index);

                if (tabWidget->currentIndex() == index)
                    syncVideoSettingsToDevices();
            });
        }
        else if (text == "连接方式") {
            QStringList list = {"默认", "USB优先", "WIFI优先"};
            addOptionMenu(&menu, text, list, &connectionMethod, [=]() {
                saveTabs(index);
            });
        }
        else if (text == "自动连接局域网设备") {
            QStringList list = {"默认", "关闭", "开启"};
            addOptionMenu(&menu, text, list, &autoScanLANDevices, [=]() {
                saveTabs(index);
            });
        }
        else if (text == "自动连接USB设备") {
            QStringList list = {"默认", "关闭", "开启"};
            addOptionMenu(&menu, text, list, &autoConnectUSBDevices, [=]() {
                saveTabs(index);
            });
        }
    }

    menu.exec(tabWidget->tabBar()->mapToGlobal(pos));
}

void MainWindow::loadTabs()
{
    int size = settings->beginReadArray("tabs");
    for (int i = 0; i < size; ++i) {
        BitMaskEditorDialog::Item item;
        item.load(i);
        tabs.append(item);
        tabWidget->addTab(new QWidget(), item.name);
    }
    settings->endArray();

    if (tabWidget->count() == 0) {
        auto name = "默认分组";
        tabs.append(BitMaskEditorDialog::Item({0, name}));
        tabWidget->addTab(new QWidget(), name);
    }
}

void MainWindow::saveTabs(int index)
{
    settings->beginWriteArray("tabs");

    int start = 0;
    int end = tabs.size();

    if (index != -1) {
        start = index;
        end = index + 1;
    }
    
    for (int i = start; i < end; i++) {
        tabs[i].save(i);
    }

    settings->endArray();
}

int MainWindow::findAvailableTabId()
{
    QSet<int> usedIds;
    for (const auto &tab : std::as_const(tabs)) {
        usedIds.insert(tab.bit);
    }

    for (int bit = 0; bit < 32; ++bit) {
        if (!usedIds.contains(bit)) {
            return bit;
        }
    }

    return -1;
}
