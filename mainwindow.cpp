#include "MainWindow.h"
#include "Tools.h"
#include "CenteredItemDelegate.h"
#include "RemoteFileExplorer.h"
#include "EventHub.h"
#include "LiveStreamDevice.h"
#include "UsbDeviceManager.h"
#include "TcpServer.h"
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
#include <QScrollArea>

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

    connect(sideBarList, &QListWidget::itemClicked, this, [=](QListWidgetItem *item) {
        if (item == item1) {
            if (qrDialog) {
                qrDialog->activateWindow();
                return;
            }

            auto img = Tools::generateQrImage(QJsonDocument(TcpServer::getInstance()->getHostInfo()).toJson().toBase64());

            qrDialog = new QDialog(this);
            qrDialog->setWindowTitle("扫码连接");
            qrDialog->setAttribute(Qt::WA_DeleteOnClose);

            QLabel *label = new QLabel(qrDialog);
            label->setPixmap(QPixmap::fromImage(img));

            auto *layout = new QVBoxLayout(qrDialog);
            layout->addWidget(label);
            qrDialog->resize(img.width(), img.height());

            connect(qrDialog, &QDialog::finished, this, [this]() {
                qrDialog = nullptr;
            });

            qrDialog->show();
        }
    });

    splitter->addWidget(sideBarList);

    auto tabWidget = new QTabWidget(this);

    auto makeScrollTab = [this]() -> QWidget* {
        auto scroll = new QScrollArea(this);
        scroll->setWidgetResizable(true);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        auto content = new QWidget;
        auto grid = new QGridLayout(content);
        scroll->setWidget(content);
        return scroll;
    };

    auto tab1 = makeScrollTab();
    auto tab2 = makeScrollTab();
    auto tab3 = makeScrollTab();

    tabWidget->addTab(tab1, "Page 1");
    tabWidget->addTab(tab2, "Page 2");
    tabWidget->addTab(tab3, "Page 3");

    connect(tabWidget->tabBar(), &QTabBar::tabBarClicked, this, &MainWindow::onTabClicked);

    splitter->addWidget(tabWidget);

    QSize screenSize = QGuiApplication::primaryScreen()->size();
    resize(screenSize.width() * 0.8, screenSize.height() * 0.8);

    EventHub::on(this, "deviceInfo", [this](const QJsonValue &data, DeviceConnection* connection) {
        connection->deviceInfo = new DeviceInfo(data.toObject());
        addItem(connection);
    });
}

MainWindow::~MainWindow()
{
    EventHub::off(this, "deviceInfo");
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

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    relayoutDevices();
}

void MainWindow::relayoutDevices()
{
    auto tabWidget = findChild<QTabWidget*>();
    if (tabWidget->currentIndex() != 0)
        return;

    auto scrollArea = qobject_cast<QScrollArea*>(tabWidget->currentWidget());

    auto contentWidget = scrollArea->widget();
    auto gridLayout = qobject_cast<QGridLayout*>(contentWidget->layout());

    int tabWidth = scrollArea->viewport()->width();

    int totalCols = std::max(1, tabWidth / (minItemWidth + spacing));

    for (int i = 0; i < devices.size(); ++i) {
        int row = i / totalCols;
        int col = i % totalCols;
        gridLayout->addWidget(devices[i], row, col);
    }
}

void MainWindow::addItem(DeviceConnection* connection)
{
    auto deviceInfo = connection->deviceInfo;

    auto player = new DeviceWidget(connection, deviceInfo);

    if (connection->type == DeviceConnection::Usb)
    {
        auto device = new LiveStreamDevice(nullptr, 0, this);
        auto manager = UsbDeviceManager::instance();
        auto ctx = manager->getContext(connection);
        manager->connectDevice(ctx->udid, deviceInfo->videoPort, [=](DeviceConnection* conn, const QByteArray& data){
            device->appendData(data);
        });

        player->setSourceDevice(device);
    }
    else
    {
        // auto url = QString("tcp://%1:%2").arg(deviceInfo->localIp).arg(deviceInfo->videoPort);
        // player->setSource(url);

        auto device = new LiveStreamDevice(deviceInfo->localIp, deviceInfo->videoPort, this);
        player->setSourceDevice(device);
    }

    auto tabWidget = findChild<QTabWidget*>();
    auto targetTab = tabWidget->widget(0);
    auto scrollArea = qobject_cast<QScrollArea*>(targetTab);
    auto contentWidget = scrollArea->widget();
    auto gridLayout = qobject_cast<QGridLayout*>(contentWidget->layout());

    auto frame = new QFrame(contentWidget);
    frame->setMinimumSize(minItemWidth, minItemHeight);
    frame->setMaximumSize(minItemWidth * 1.5, minItemHeight * 1.5);
    frame->setFrameShape(QFrame::Box);
    auto frameLayout = new QVBoxLayout(frame);
    frameLayout->setContentsMargins(0, 0, 0, 0);
    frameLayout->addWidget(player);

    devices.append(frame);

    relayoutDevices();
}
