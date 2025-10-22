#include "MainWindow.h"
#include "Logger.h"
#include "CenteredItemDelegate.h"
#include "RemoteFileExplorer.h"
#include "EventHub.h"
#include "LiveStreamDevice.h"
#include "UsbDeviceManager.h"
#include <QTabWidget>
#include <QWidget>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFrame>
#include <QGuiApplication>
#include <QScreen>
#include <QApplication>
#include <QTabBar>
#include "DeviceWidget.h"
#include <QSplitter>
#include <QListWidget>
#include <QListWidgetItem>
#include <QStyle>
#include <QIcon>
#include <QScrollArea>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    auto central = new QWidget(this);
    auto mainLayout = new QHBoxLayout(central);

    auto splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setStyleSheet("QSplitter::handle {"
                            "background-color: #B0B0B0;"
                            "width: 1px;"
                            "}");

    // 左侧导航栏
    auto sideBarWidget = new QWidget(this);
    sideBarWidget->setFixedWidth(70);
    sideBarWidget->setStyleSheet("background: transparent; border: none;");

    auto sideBarLayout = new QVBoxLayout(sideBarWidget);

    auto sideBarList = new QListWidget(sideBarWidget);
    sideBarList->setViewMode(QListView::ListMode);
    sideBarList->setIconSize(QSize(36, 36));
    sideBarList->setSpacing(5);
    sideBarList->setItemDelegate(new CenteredItemDelegate(this));
    sideBarList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto style = sideBarWidget->style();

    // auto item1 = new QListWidgetItem(QIcon::fromTheme("user-home"), "");
    auto item1 = new QListWidgetItem(QIcon(style->standardIcon(QStyle::SP_DesktopIcon)), "");
    item1->setToolTip("提示1");
    auto item2 = new QListWidgetItem(QIcon(style->standardIcon(QStyle::SP_ComputerIcon)), "");
    item2->setToolTip("提示2");
    auto item3 = new QListWidgetItem(QIcon(style->standardIcon(QStyle::SP_BrowserReload)), "");
    item3->setToolTip("提示3");
    auto item4 = new QListWidgetItem(QIcon(style->standardIcon(QStyle::SP_FileIcon)), "");
    item4->setToolTip("提示4");

    sideBarList->addItem(item1);
    sideBarList->addItem(item2);
    sideBarList->addItem(item3);
    sideBarList->addItem(item4);
    sideBarLayout->addWidget(sideBarList);

    splitter->addWidget(sideBarWidget);

    // 右侧区域
    auto rightWidget = new QWidget(this);
    auto rightLayout = new QVBoxLayout(rightWidget);

    // 上方 Tab 区域
    auto tabWidget = new QTabWidget(this);
    auto tab1 = new QWidget();
    auto tab2 = new QWidget();
    auto tab3 = new QWidget();
    tab1->setLayout(new QVBoxLayout());
    tab1->layout()->addWidget(new QLabel("这是标签页 2 的内容"));
    tab2->setLayout(new QVBoxLayout());
    tab2->layout()->addWidget(new QLabel("这是标签页 2 的内容"));
    tab3->setLayout(new QVBoxLayout());
    tab3->layout()->addWidget(new QLabel("这是标签页 3 的内容"));

    tabWidget->addTab(tab1, "Tab 1");
    tabWidget->addTab(tab2, "Tab 2");
    tabWidget->addTab(tab3, "Tab 3");

    connect(tabWidget->tabBar(), &QTabBar::tabBarClicked, this, &MainWindow::onTabClicked);

    // ==== 下方区域（可滚动） ====
    bottomWidget = new QWidget(this);
    gridLayout = new QGridLayout(bottomWidget);
    gridLayout->setSpacing(0);
    gridLayout->setContentsMargins(0, 0, 0, 0);

    auto scrollArea = new QScrollArea(this);
    scrollArea->setWidget(bottomWidget);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    rightLayout->addWidget(tabWidget, 2);
    rightLayout->addWidget(scrollArea, 1);

    splitter->addWidget(rightWidget);
    mainLayout->addWidget(splitter);

    setCentralWidget(central);

    QSize screenSize = QGuiApplication::primaryScreen()->size();
    resize(screenSize.width() * 0.8, screenSize.height() * 0.8);

    EventHub::StartListening("deviceInfo", [this](const QJsonValue &data, DeviceConnection* connection) {
        auto deviceInfo = new DeviceInfo(data.toObject());
        addItem(connection, deviceInfo);
    });
}

MainWindow::~MainWindow()
{
    EventHub::StopListening("deviceInfo");
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QApplication::quit();
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
    {
        close();
    }
    else
    {
        QMainWindow::keyPressEvent(event);
    }
}

void MainWindow::onTabClicked(int index)
{
    qDebugEx() << "Clicked on Tab " << index + 1;
    switch (index)
    {
    case 0:
        qDebugEx() << "Tab 1 clicked: Video Player tab";
        break;
    case 1:
        qDebugEx() << "Tab 2 clicked: Content for Tab 2";
        break;
    case 2:
        qDebugEx() << "Tab 3 clicked: Content for Tab 3";
        break;
    default:
        break;
    }
}

void MainWindow::addItem(DeviceConnection* connection, DeviceInfo* deviceInfo)
{
    auto url = deviceInfo ? QString("tcp://%1:%2").arg(deviceInfo->localIp).arg(deviceInfo->videoPort) : nullptr;

    LiveStreamDevice* liveStreamDevice = nullptr;
    
    if (connection->type == DeviceConnection::Usb)
    {
        liveStreamDevice = new LiveStreamDevice();

        auto manager = UsbDeviceManager::instance();
        auto ctx = manager->getContext(connection);

        manager->connectDevice(ctx->udid, deviceInfo->videoPort, [=](DeviceConnection* conn, const QByteArray& data){
            liveStreamDevice->appendData(data);
        });
    }

    int count = gridLayout->count();

    if (url != nullptr) {
        // 遍历查找第一个占位
        for (int i = 0; i < count; i++) {
            auto frame = qobject_cast<QFrame*>(gridLayout->itemAt(i)->widget());
            if (!frame) continue;

            auto frameLayout = qobject_cast<QVBoxLayout*>(frame->layout());
            if (!frameLayout || frameLayout->count() == 0) continue;

            auto w = frameLayout->itemAt(0)->widget();

            if (qobject_cast<DeviceWidget*>(w) == nullptr) {
                qDebugEx() << "找到占位" << i;
                delete w;
                auto player = new DeviceWidget(connection, deviceInfo);
                if (liveStreamDevice)
                    player->setSourceDevice(liveStreamDevice);
                else
                    player->setSource(url);
                frameLayout->addWidget(player);
                bottomWidget->adjustSize();
                return;
            }
        }
    }

    // 如果没有占位，说明要新开一行
    int totalCols = 6;
    int row = count / totalCols;

    for (int i = 0; i < totalCols; i++) {
        auto frame = new QFrame(bottomWidget);
        frame->setFrameShape(QFrame::Box);

        auto frameLayout = new QVBoxLayout(frame);
        frameLayout->setContentsMargins(0, 0, 0, 0);

        if (i == 0 && url != nullptr) {
            qDebugEx() << "放在新行第一个" << url;
            auto player = new DeviceWidget(connection, deviceInfo);
            if (liveStreamDevice)
                player->setSourceDevice(liveStreamDevice);
            else
                player->setSource(url);
            frameLayout->addWidget(player);
        } else {
            // 其他先放占位
            frameLayout->addWidget(new QWidget());
        }

        gridLayout->addWidget(frame, row, i);
    }

    bottomWidget->adjustSize();

    if (bottomWidget->height() < 1000)
        gridLayout->setRowMinimumHeight(row, bottomWidget->height());
}
