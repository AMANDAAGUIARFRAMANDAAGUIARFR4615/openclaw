#include "MainWindow.h"
#include "Tools.h"
#include "CenteredItemDelegate.h"
#include "RemoteFileExplorer.h"
#include "EventHub.h"
#include "LiveStreamDevice.h"
#include "UsbDeviceManager.h"
#include "TcpServer.h"
#include "DeviceWidget.h"
#include "DeviceManager.h"
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

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
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

    auto style = sideBarList->style();

    sideBarList->addItem(new QListWidgetItem(QIcon(style->standardIcon(QStyle::SP_DesktopIcon)), "设备连接"));
    sideBarList->addItem(new QListWidgetItem(QIcon(style->standardIcon(QStyle::SP_ComputerIcon)), "分组操作"));
    sideBarList->addItem(new QListWidgetItem(QIcon(style->standardIcon(QStyle::SP_BrowserReload)), "分组设置"));
    sideBarList->addItem(new QListWidgetItem(QIcon(style->standardIcon(QStyle::SP_FileIcon)), "关于"));

    for (int i = 0; i < sideBarList->count(); i++) {
        sideBarList->item(i)->setSizeHint(QSize(sideBarList->width() - 4, 70));
    }

    connect(sideBarList, &QListWidget::itemClicked, this, [=](QListWidgetItem *item) {
        QString text = item->text();

        if (text == "设备连接") {
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
            return;
        }

        if (text == "分组操作") {
            QMenu *menu = new QMenu(this);
            menu->addAction("选项 1");
            menu->addAction("选项 2");
            menu->addAction("选项 3");
            QRect rect = sideBarList->visualItemRect(item);
            QPoint globalPos = sideBarList->viewport()->mapToGlobal(rect.topRight());
            menu->exec(globalPos);
            delete menu;
            return;
        }

        if (text == "分组设置") {
            auto window = new DeviceManager();
            window->show();
            return;
        }
    });

    splitter->addWidget(sideBarList);

    tabWidget = new QTabWidget(this);
    auto tabBar = tabWidget->tabBar();
    tabBar->setMovable(true);
    tabBar->setToolTip("右键点击可修改分组");
    tabBar->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(tabWidget, &QTabWidget::tabBarClicked, this, &MainWindow::onTabClicked);
    connect(tabBar, &QWidget::customContextMenuRequested, this, &MainWindow::showTabBarContextMenu);

    splitter->addWidget(tabWidget);

    scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    auto content = new QWidget;
    auto gridLayout = new QGridLayout(content);
    gridLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    scrollArea->setWidget(content);

    tabWidget->addTab(scrollArea, "默认分组");

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
    auto contentWidget = scrollArea->widget();
    auto gridLayout = qobject_cast<QGridLayout*>(contentWidget->layout());

    int tabWidth = scrollArea->viewport()->width();

    int totalCols = std::max(1, tabWidth / (minItemWidth + spacing));

    if (devices.count() >= totalCols)
        gridLayout->setAlignment(Qt::AlignHCenter | Qt::AlignTop);

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

    auto frame = new QFrame(this);
    frame->setMinimumSize(minItemWidth, minItemHeight);
    frame->setMaximumSize(minItemWidth * 1.5, minItemHeight * 1.5);
    frame->setFrameShape(QFrame::Box);
    auto frameLayout = new QVBoxLayout(frame);
    frameLayout->setContentsMargins(0, 0, 0, 0);
    frameLayout->addWidget(player);

    devices.append(frame);

    relayoutDevices();
}

void MainWindow::showTabBarContextMenu(const QPoint &pos)
{
    int index = tabWidget->tabBar()->tabAt(pos);
    if (index < 0)
        return;

    QMenu menu(this);

    connect(menu.addAction("重命名"), &QAction::triggered, this, [=]() {
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
        }
    });
    connect(menu.addAction("删除"), &QAction::triggered, this, [=]() {
        QWidget *page = tabWidget->widget(index);
        tabWidget->removeTab(index);
        delete page;
    });
    connect(menu.addAction("添加"), &QAction::triggered, this, [=]() {
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
            QWidget *newTab = new QWidget();
            tabWidget->insertTab(index + 1, nullptr, newName.trimmed());
        }
    });

    menu.exec(tabWidget->tabBar()->mapToGlobal(pos));
}
