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
#include "DeviceManager.h"
#include "ToastWidget.h"
#include "SettingsViewer.h"
#include "FileTransfer.h"
#include <QShortcut>
#include <QTabWidget>
#include <QWidget>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFrame>
#include <QApplication>
#include <QScreen>
#include <QApplication>
#include <QTabBar>
#include <QSplitter>
#include <QListWidget>
#include <QListWidgetItem>
#include <QStyle>
#include <QIcon>
#include <QTimer>
#include <cmath>
#include <QScrollArea>
#include <QMenu>
#include <QDialog>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QFormLayout>
#include <QInputDialog>
#include <QFileDialog>
#include <QJsonObject>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    QSize screenSize = QApplication::primaryScreen()->size();
    resize(screenSize.width() * 0.8, screenSize.height() * 0.8);

    int x = (screenSize.width() - width()) / 2;
    int y = (screenSize.height() - height()) / 2;
    move(x, y);

    auto central = new QWidget(this);
    setCentralWidget(central);

    auto mainLayout = new QHBoxLayout(central);

    auto splitter = new QSplitter(Qt::Horizontal, this);
    mainLayout->addWidget(splitter);
    splitter->setStyleSheet("QSplitter::handle {background-color: #B0B0B0;width: 1px;}");

    auto sideBarList = new QListWidget();
    sideBarList->setViewMode(QListView::IconMode);
    sideBarList->setFixedWidth(80);
    sideBarList->setStyleSheet("QListWidget::item { margin-top: 10px; margin-bottom: 10px; }");

    sideBarList->addItem(new QListWidgetItem(EmojiIconProvider::createIcon("🔗"), "设备连接"));
    sideBarList->addItem(new QListWidgetItem(EmojiIconProvider::createIcon("👥"), "分组群控"));
    sideBarList->addItem(new QListWidgetItem(EmojiIconProvider::createIcon("📱"), "设备列表"));
    sideBarList->addItem(new QListWidgetItem(EmojiIconProvider::createIcon("⚙️"), "设置"));
    sideBarList->addItem(new QListWidgetItem(EmojiIconProvider::createIcon("💡"), "帮助"));
    sideBarList->addItem(new QListWidgetItem(EmojiIconProvider::createIcon("💬"), "客服"));
    sideBarList->addItem(new QListWidgetItem(EmojiIconProvider::createIcon("📜"), "日志"));
    sideBarList->addItem(new QListWidgetItem(EmojiIconProvider::createIcon("🛠️"), "开发者"));

    for (int i = 0; i < sideBarList->count(); i++) {
        sideBarList->item(i)->setSizeHint(QSize(sideBarList->width() - 4, 70));
    }

    connect(sideBarList, &QListWidget::itemClicked, [=](QListWidgetItem *item) {
        QString text = item->text();

        if (text == "设备连接") {
            auto img = Tools::generateQrImage(QJsonDocument(TcpServer::getInstance()->getHostInfo()).toJson().toBase64());

            auto qrDialog = new QDialog(this);
            qrDialog->setWindowTitle("扫码连接");

            QLabel *label = new QLabel(qrDialog);
            label->setPixmap(QPixmap::fromImage(img));

            auto layout = new QVBoxLayout(qrDialog);
            layout->addWidget(label);
            qrDialog->resize(img.width(), img.height());

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

        if (text == "设备列表") {
            // auto window = new DeviceManager();
            // window->show();

            auto socket = new QTcpSocket();
            connect(socket, &QTcpSocket::errorOccurred, [=](QAbstractSocket::SocketError socketError) {
                qCriticalEx() << "errorOccurred" << socketError << socket->errorString();
            });
            
            connect(socket, &QTcpSocket::readyRead, [=]() {
                qDebugEx() << "收到数据1111";
            });

            socket->connectToHost("119.28.3.242", 55257);
            return;
        }

        if (text == "帮助") {
            auto socket = new QTcpSocket();
            connect(socket, &QTcpSocket::errorOccurred, [=](QAbstractSocket::SocketError socketError) {
                qCriticalEx() << "errorOccurred" << socketError << socket->errorString();
            });
            
            connect(socket, &QTcpSocket::readyRead, [=]() {
                qDebugEx() << "收到数据2222";
            });

            socket->connectToHost("192.168.0.111", 56504);
            return;
        }

        if (text == "客服") {
            qDebugEx() << "发起ipv6请求";

            auto socket = new QTcpSocket();
            connect(socket, &QTcpSocket::errorOccurred, [=](QAbstractSocket::SocketError socketError) {
                qCriticalEx() << "errorOccurred" << socketError << socket->errorString();
            });

            connect(socket, &QTcpSocket::connected, [=]() {
                qDebugEx() << "连接成功";
            });

            socket->connectToHost("2409:8a34:452:950:aa32:471b:f039:b06e", 12345);
            return;
        }

        if (text == "日志") {
            auto logWindow = LogWindow::getInstance();
            logWindow->setParent(scrollArea);
            logWindow->resize(scrollArea->size());
            logWindow->toggleVisibility();
            return;
        }

        if (text == "开发者") {
            new SettingsViewer(&settings, this);
            return;
        }
    });

    splitter->addWidget(sideBarList);

    tabWidget = new QTabWidget(this);
    auto tabBar = tabWidget->tabBar();
    tabBar->setMovable(true);
    tabBar->setToolTip("右键点击可修改分组");
    tabBar->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(tabBar, &QTabBar::currentChanged, this, &MainWindow::onTabChanged);
    connect(tabBar, &QTabBar::tabMoved, this, &MainWindow::onTabMoved);
    connect(tabBar, &QWidget::customContextMenuRequested, this, &MainWindow::showTabBarContextMenu);

    splitter->addWidget(tabWidget);

    scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    auto content = new QWidget;
    auto gridLayout = new QGridLayout(content);
    scrollArea->setWidget(content);

    EventHub::on(this, "deviceInfo", [this](const QJsonValue &data, DeviceConnection* connection) {
        connection->deviceInfo = new DeviceInfo(connection, data.toObject());
        addItem(connection);
    });

    EventHub::on(this, "disconnected", [this](const QJsonValue &data, DeviceConnection* connection) {
        qDebugEx() << "断开连接处理";
        deviceFrames.remove(connection->deviceInfo);
        delete connection->deviceInfo;
        relayoutDevices();
    });

    loadTabs();
}

MainWindow::~MainWindow()
{
    EventHub::off(this, "deviceInfo");
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveTabs();
    QApplication::quit();
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
        close();
    else
        QMainWindow::keyPressEvent(event);
}

void MainWindow::onTabChanged(int index)
{
    qDebugEx() << "onTabChanged" << index;

    auto widget = tabWidget->currentWidget();
    auto layout = widget->layout();
    if (!layout) {
        layout = new QVBoxLayout(widget);
        layout->setContentsMargins(0, 0, 0, 0);
    }
    layout->addWidget(scrollArea);

    relayoutDevices();
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
    auto contentWidget = scrollArea->widget();
    auto gridLayout = qobject_cast<QGridLayout*>(contentWidget->layout());

    int tabWidth = scrollArea->viewport()->width();

    int totalCols = std::max(1, tabWidth / (frameItemWidth + spacing));

    auto bit = tabs[tabWidget->currentIndex()].bit;
    auto devices = DeviceInfo::getDevices(bit == 0 ? 0 : (1U << bit));

    if (devices.count() > totalCols)
        gridLayout->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    else
        gridLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    while (QLayoutItem* item = gridLayout->takeAt(0)) {
        if (auto w = item->widget())
            w->hide();
        delete item;
    }

    for (int i = 0; i < devices.size(); ++i) {
        int row = i / totalCols;
        int col = i % totalCols;

        auto widget = deviceFrames[devices[i]];
        widget->show();
        gridLayout->addWidget(widget, row, col);
    }
}

void MainWindow::addItem(DeviceConnection* connection)
{
    auto deviceInfo = connection->deviceInfo;

    auto player = new DeviceWidget(connection, deviceInfo);

    if (connection->type == DeviceConnection::Usb)
    {
        auto device = new LiveStreamDevice(nullptr, 0, this);

        auto ctx = g_usbDeviceManager->getContext(connection);
        g_usbDeviceManager->connectDevice(ctx->udid, deviceInfo->videoPort, [=](DeviceConnection* conn, const QByteArray& data){
            device->appendData(data);
        });

        player->setSourceDevice(device);
    }
    else
    {
        auto device = new LiveStreamDevice(deviceInfo->localIp, deviceInfo->videoPort, this);
        player->setSourceDevice(device);
    }

    auto frame = new QFrame(this);
    frame->setFixedSize(frameItemWidth, frameItemHeight);
    frame->setFrameShape(QFrame::Box);
    auto frameLayout = new QVBoxLayout(frame);
    frameLayout->setContentsMargins(0, 0, 0, 0);
    frameLayout->addWidget(player);

    deviceFrames.insert(deviceInfo, frame);

    relayoutDevices();
}

void MainWindow::showTabBarContextMenu(const QPoint &pos)
{
    int index = tabWidget->tabBar()->tabAt(pos);
    if (index < 0)
        return;

    auto bit = tabs[index].bit;

    QMenu menu(this);

    menu.addAction("重命名", [=]() {
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
    
    menu.addAction("删除", [=]() {
        QWidget *page = tabWidget->widget(index);
        tabWidget->removeTab(index);
        delete page;
        tabs.remove(index);
    })->setEnabled(bit != 0);

    menu.addAction("添加", [=]() {
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

    menu.exec(tabWidget->tabBar()->mapToGlobal(pos));
}

void MainWindow::loadTabs()
{
    QVariantList tabList = settings.value("tabs").toList();
    for (const auto& var : tabList) {
        QVariantMap map = var.toMap();
        int bit = map["bit"].toInt();
        QString name = map["name"].toString();
        addTab(bit, name);
    }

    if (tabWidget->count() == 0)
        addTab(0, "默认分组");
}

void MainWindow::saveTabs()
{
    QVariantList tabList;
    for (const auto& tab : tabs) {
        QVariantMap map;
        map["bit"] = tab.bit;
        map["name"] = tab.name;
        tabList.append(map);
    }
    settings.setValue("tabs", tabList);
}

void MainWindow::addTab(int bit, const QString &name)
{
    tabs.append(BitMaskEditorDialog::Item({bit, name}));
    tabWidget->addTab(new QWidget(), name);
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
