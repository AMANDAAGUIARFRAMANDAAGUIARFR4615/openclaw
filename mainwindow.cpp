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
    setCentralWidget(central);
    
    auto mainLayout = new QHBoxLayout(central);

    auto splitter = new QSplitter(Qt::Horizontal, this);
    mainLayout->addWidget(splitter);
    splitter->setStyleSheet("QSplitter::handle {"
                           "background-color: #B0B0B0;"
                           "width: 1px;"
                           "}");

    auto sideBarList = new QListWidget();
    sideBarList->setIconSize(QSize(36, 36));
    sideBarList->setSpacing(5);
    sideBarList->setItemDelegate(new CenteredItemDelegate(this));
    sideBarList->setFixedWidth(70);

    auto style = sideBarList->style();

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

    splitter->addWidget(sideBarList);

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

    splitter->addWidget(tabWidget);

    QSize screenSize = QGuiApplication::primaryScreen()->size();
    resize(screenSize.width() * 0.8, screenSize.height() * 0.8);

    EventHub::StartListening("deviceInfo", [this](const QJsonValue &data, DeviceConnection* connection) {
        connection->deviceInfo = new DeviceInfo(data.toObject());
        for(int i = 0; i < 13; i++)
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
        close();
    else
        QMainWindow::keyPressEvent(event);
}

void MainWindow::onTabClicked(int index)
{
    qDebugEx() << "Clicked on Tab " << index + 1;
}

void MainWindow::addItem(DeviceConnection* connection)
{
    auto tabWidget = findChild<QTabWidget*>();

    auto deviceInfo = connection->deviceInfo;

    auto player = new DeviceWidget(connection, deviceInfo);

    if (connection->type == DeviceConnection::Usb)
    {
        auto liveStreamDevice = new LiveStreamDevice();
        auto manager = UsbDeviceManager::instance();
        auto ctx = manager->getContext(connection);
        manager->connectDevice(ctx->udid, deviceInfo->videoPort, [=](DeviceConnection* conn, const QByteArray& data){
            liveStreamDevice->appendData(data);
        });

        player->setSourceDevice(liveStreamDevice);
    }
    else
    {
        auto url = QString("tcp://%1:%2").arg(deviceInfo->localIp).arg(deviceInfo->videoPort);
        player->setSource(url);
    }

    auto targetTab = tabWidget->widget(0);
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

    int tabWidth = tabWidget->width();
    int tabHeight = tabWidget->height();

    int maxItemWidth = 200;
    int maxItemHeight = maxItemWidth * 1.7786;
    int spacing = 10;

    int totalCols = std::max(1, tabWidth / (maxItemWidth + spacing));
    int totalRows = std::max(1, tabHeight / (maxItemHeight + spacing));

    for (int r = 0; r < totalRows; ++r) {
        for (int c = 0; c < totalCols; ++c) {
            if (gridLayout->itemAtPosition(r, c) == nullptr) {
                gridLayout->addWidget(new QWidget(), r, c);
            }
        }
    }

    int row = itemsPerTab / totalCols;
    int col = itemsPerTab % totalCols;

    auto frame = new QFrame(tabWidget);
    frame->setMinimumSize(maxItemWidth / 2, maxItemHeight / 2);
    frame->setMaximumSize(maxItemWidth, maxItemHeight);
    frame->setFrameShape(QFrame::Box);
    auto frameLayout = new QVBoxLayout(frame);
    frameLayout->setContentsMargins(0, 0, 0, 0);
    frameLayout->addWidget(player);

    gridLayout->addWidget(frame, row, col);
    gridLayout->update();
}
