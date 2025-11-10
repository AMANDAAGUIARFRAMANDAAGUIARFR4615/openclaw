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

    // auto sideBarList2 = new QListWidget();
    // sideBarList2->setViewMode(QListView::IconMode);
    // sideBarList2->setFixedWidth(60);
    // sideBarList2->setStyleSheet("QListWidget::item { margin-top: 10px; margin-bottom: 10px; }");

    // sideBarList2->addItem(new QListWidgetItem(QIcon(":/icons/unlock.png"), "解锁"));
    // sideBarList2->addItem(new QListWidgetItem(QIcon(":/icons/lock.png"), "锁屏"));
    // sideBarList2->addItem(new QListWidgetItem(QIcon(":/icons/restart.png"), "重启"));
    // sideBarList2->addItem(new QListWidgetItem(QIcon(":/icons/kill.png"), "清理应用"));

    // for (int i = 0; i < sideBarList2->count(); i++) {
    //     sideBarList2->item(i)->setSizeHint(QSize(sideBarList2->width() - 4, 70));
    // }

    // splitter->addWidget(sideBarList2);

    tabWidget = new QTabWidget(this);
    tabWidget->tabBar()->setMovable(true);
    tabWidget->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(tabWidget, &QTabWidget::tabBarClicked, this, &MainWindow::onTabClicked);
    connect(tabWidget->tabBar(), &QWidget::customContextMenuRequested, this, &MainWindow::showTabManager);

    splitter->addWidget(tabWidget);

    auto makeScrollTab = [this]() -> QWidget* {
        auto scroll = new QScrollArea(this);
        scroll->setWidgetResizable(true);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        auto content = new QWidget;
        auto grid = new QGridLayout(content);
        grid->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        scroll->setWidget(content);
        return scroll;
    };

    for (int i = 0; i < 3; ++i) {
        auto tab = makeScrollTab();
        tabWidget->addTab(tab, QString("Page %1").arg(i+1));
    }

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

uint32_t tabVisibleBits = 0xFFFFFFFF; // 默认全部显示，32位

void MainWindow::showTabManager(const QPoint &pos)
{
    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle("管理 Tabs");
    dlg->resize(400, 500);

    QVBoxLayout *mainLayout = new QVBoxLayout(dlg);

    QScrollArea *scroll = new QScrollArea(dlg);
    scroll->setWidgetResizable(true);
    mainLayout->addWidget(scroll);

    QWidget *container = new QWidget;
    QVBoxLayout *vLayout = new QVBoxLayout(container);
    vLayout->setAlignment(Qt::AlignTop);
    scroll->setWidget(container);

    // 保存每行控件，方便确定按钮读取
    struct TabRow { QLineEdit* nameEdit; QCheckBox* check; };
    QList<TabRow> rows;

    int count = std::min(tabWidget->count(), 32);
    for (int i = 0; i < count; ++i) {
        QWidget *rowWidget = new QWidget;
        QHBoxLayout *hLayout = new QHBoxLayout(rowWidget);
        hLayout->setContentsMargins(5, 2, 5, 2);

        QLineEdit *nameEdit = new QLineEdit(tabWidget->tabText(i));
        QCheckBox *check = new QCheckBox("显示");
        check->setChecked(tabWidget->isTabEnabled(i));

        hLayout->addWidget(nameEdit);
        hLayout->addWidget(check);
        vLayout->addWidget(rowWidget);

        rows.append({nameEdit, check});
    }

    QPushButton *okBtn = new QPushButton("确定");
    mainLayout->addWidget(okBtn);

    connect(okBtn, &QPushButton::clicked, dlg, [this, dlg, rows]() {
        for (int i = 0; i < rows.size(); ++i) {
            tabWidget->setTabText(i, rows[i].nameEdit->text());
            tabWidget->setTabEnabled(i, rows[i].check->isChecked());
        }
        dlg->accept();
    });

    dlg->exec();
    delete dlg;
}
