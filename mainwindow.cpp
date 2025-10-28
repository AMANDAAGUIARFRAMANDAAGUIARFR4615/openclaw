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
#include <QTimer>
#include <cmath>

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
    rightWidget->setObjectName("rightWidget");
    auto rightLayout = new QVBoxLayout(rightWidget);

    // Tab 区域
    auto tabWidget = new QTabWidget(this);
    auto tab1 = new QWidget();
    auto tab2 = new QWidget();
    auto tab3 = new QWidget();
    tab1->setLayout(new QGridLayout());
    tab2->setLayout(new QGridLayout());
    tab3->setLayout(new QGridLayout());

    tabWidget->addTab(tab1, "Page 1");
    tabWidget->addTab(tab2, "Page 2");
    tabWidget->addTab(tab3, "Page 3");

    connect(tabWidget->tabBar(), &QTabBar::tabBarClicked, this, &MainWindow::onTabClicked);

    rightLayout->addWidget(tabWidget, 1);
    rightWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    splitter->addWidget(rightWidget);
    mainLayout->addWidget(splitter);

    setCentralWidget(central);

    QSize screenSize = QGuiApplication::primaryScreen()->size();
    resize(screenSize.width() * 0.8, screenSize.height() * 0.8);

    EventHub::StartListening("deviceInfo", [this](const QJsonValue &data, DeviceConnection* connection) {
        connection->deviceInfo = new DeviceInfo(data.toObject());

        for(int i = 0; i < 50; i++)
        addItem(connection);
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

void MainWindow::addItem(DeviceConnection* connection)
{
    auto rightWidget = findChild<QWidget*>("rightWidget");
    if (!rightWidget) {
        qDebugEx() << "Error: rightWidget not found";
        return;
    }
    auto rightLayout = qobject_cast<QVBoxLayout*>(rightWidget->layout());
    if (!rightLayout) {
        qDebugEx() << "Error: rightLayout not found";
        return;
    }
    auto tabWidget = rightWidget->findChild<QTabWidget*>();
    if (!tabWidget) {
        qDebugEx() << "Error: tabWidget not found";
        return;
    }

    // 计算当前总设备数量
    int totalCount = 0;
    for (int i = 0; i < tabWidget->count(); ++i) {
        auto tab = tabWidget->widget(i);
        auto gridLayout = qobject_cast<QGridLayout*>(tab->layout());
        for (int j = 0; j < gridLayout->count(); ++j) {
            auto frame = qobject_cast<QFrame*>(gridLayout->itemAt(j)->widget());
            if (frame) {
                auto frameLayout = qobject_cast<QVBoxLayout*>(frame->layout());
                if (frameLayout && frameLayout->count() > 0) {
                    auto w = frameLayout->itemAt(0)->widget();
                    if (qobject_cast<DeviceWidget*>(w)) {
                        totalCount++;
                    }
                }
            }
        }
    }

    // 限制最多 100 个设备
    if (totalCount >= 100) {
        qDebugEx() << "已达到最大设备数量 (100)，无法添加更多设备";
        return;
    }

    auto deviceInfo = connection->deviceInfo;
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

    int targetTabIndex = totalCount / 50;
    if (targetTabIndex >= tabWidget->count()) {
        auto newTab = new QWidget();
        newTab->setLayout(new QGridLayout());
        newTab->layout()->setSpacing(2);
        tabWidget->addTab(newTab, QString("Page %1").arg(tabWidget->count() + 1));
    }

    auto targetTab = tabWidget->widget(targetTabIndex);
    auto gridLayout = qobject_cast<QGridLayout*>(targetTab->layout());
    int itemsPerTab = 0;
    for (int i = 0; i < gridLayout->count(); ++i) {
        auto frame = qobject_cast<QFrame*>(gridLayout->itemAt(i)->widget());
        if (frame) {
            auto frameLayout = qobject_cast<QVBoxLayout*>(frame->layout());
            if (frameLayout && frameLayout->count() > 0) {
                auto w = frameLayout->itemAt(0)->widget();
                if (qobject_cast<DeviceWidget*>(w)) {
                    itemsPerTab++;
                }
            }
        }
    }

    int totalCols = 10;
    int row = itemsPerTab / totalCols;
    int col = itemsPerTab % totalCols;

    auto frame = new QFrame(rightWidget);
    frame->setFrameShape(QFrame::Box);
    auto frameLayout = new QVBoxLayout(frame);
    frameLayout->setContentsMargins(0, 0, 0, 0);

    if (url != nullptr) {
        qDebugEx() << "Adding device at Tab " << targetTabIndex << ", row " << row << ", col " << col
                   << ", totalCount = " << totalCount << ", itemsPerTab = " << itemsPerTab;
        auto player = new DeviceWidget(connection, deviceInfo);
        auto minWidth = tabWidget->width() / totalCols - 10;
        auto minHeight = tabWidget->height() / ((50 + totalCols - 1) / totalCols) - 10;
        player->setMinimumSize(std::max(150, minWidth), std::max(150, minHeight));
        // player->setMaximumSize(std::max(150, minWidth * 4), std::max(150, minHeight * 4));
        if (liveStreamDevice)
            player->setSourceDevice(liveStreamDevice);
        else
            player->setSource(url);
        frameLayout->addWidget(player);
    } else {
        frameLayout->addWidget(new QWidget()); // 占位
    }

    gridLayout->addWidget(frame, row, col);

    // 强制更新布局
    gridLayout->update();
    targetTab->update();
    rightWidget->adjustSize();
    tabWidget->setCurrentIndex(targetTabIndex); // 切换到当前 Tab
}
