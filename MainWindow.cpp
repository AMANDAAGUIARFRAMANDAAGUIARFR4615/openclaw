#include "MainWindow.h"
#include "LogTextBrowser.h"
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
#include "RedeemRecordDialog.h"
#include "RenewalDialog.h"
#include "Account.h"
#include "AccountListDialog.h"
#include "NetworkSegmentEditorDialog.h"
#include "SwapExpirationDialog.h"
#include "DeviceWindow.h"
#include "ExplicitSelectionListWidget.h"
#include "NaturalSortListWidgetItem.h"
#include "Safe.h"
#include "VersionManagerDialog.h"
#include "FlowEditorDialog.h"
#include "VideoFrameWidget.h"
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
#include <QDesktopServices>
#include <QCalendarWidget>
#include <QMimeData>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QCloseEvent>
#include <QMouseEvent>
#include <QWindow>

// 独立屏幕悬浮窗口类
class FloatingTabWindow : public QDialog {
    Q_OBJECT
public:
    ExplicitSelectionListWidget* listWidget;
    BitMaskEditorDialog::Item tabItem;
    VideoVisibilityManager* visibilityMgr;

    FloatingTabWindow(QWidget* parent, const BitMaskEditorDialog::Item& item) 
        : QDialog(parent), tabItem(item) {
        setWindowFlags(Qt::Window | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
        setWindowTitle(item.name);

        auto layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);

        // 为该独立窗口创建专属的设备列表承载区
        listWidget = new ExplicitSelectionListWidget(this);
        listWidget->setViewMode(QListWidget::IconMode);
        listWidget->setResizeMode(QListWidget::Adjust);
        listWidget->setDragDropMode(QListWidget::NoDragDrop);
        listWidget->setSpacing(10);
        listWidget->setSortingEnabled(true);
        listWidget->sortItems(Qt::AscendingOrder);
        listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
        layout->addWidget(listWidget);

        // 独立的视频能见度管理器，确保悬浮窗上的设备正常渲染
        visibilityMgr = new VideoVisibilityManager(listWidget, this);
    }

    void reject() override {
        close(); 
    }

    void closeEvent(QCloseEvent* event) override {
        emit closed();
        event->accept();
    }

signals:
    void closed();
};

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
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

    sideBarList = new QListWidget();
    sideBarList->setViewMode(QListView::IconMode);
    sideBarList->setFixedWidth(80);
    sideBarList->setCursor(Qt::PointingHandCursor);
    sideBarList->setDragDropMode(QAbstractItemView::NoDragDrop);
    sideBarList->setStyleSheet(R"(
        QListWidget {
            border: none;
            outline: 0px;
            background-color: transparent;
        }
        QListWidget::item {
            border: 1px solid transparent; 
            border-radius: 6px;
            color: palette(text);
            padding: 4px;
        }
        QListWidget::item:hover {
            background-color: palette(midlight); 
            border: 1px solid palette(mid);
        }
        QListWidget::item:selected {
            background-color: palette(highlight);
            color: palette(highlighted-text);
            border: 1px solid palette(dark);
        }
    )");

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

    connect(AppSettingsDialog::getInstance(), &AppSettingsDialog::configurationChanged, this, [=](const QString &key) {
        if (key == "sideBarMenu")
        {
            updateSideBar();
            return;
        }

        if (key == "colorScheme") {
            new ToastWidget("修改颜色主题需要重启软件才能生效");
            return;
        }

        if (key == "sortSelectedToTop") {
            for (auto list : findChildren<ExplicitSelectionListWidget*>())
                list->sortItems(Qt::AscendingOrder);
            return;
        }

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
            NetworkSegmentEditorDialog(this).exec();
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

            helpDialog->exec();
            return;
        }

        if (title == "越狱助手") {
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
            JailbreakAssistantDialog(this).exec();
#endif
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
                {"Sileo", "sileo://source/" + Config::SITE_URL},
                {"Cydia", "cydia://url/https://cydia.saurik.com/api/share#?source=" + Config::SITE_URL}
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

        if (title == "USB驱动") {
            auto reply = QMessageBox::question(this, "需安装 iTunes",
                                  "检测到您尚未安装 iTunes（或版本过低）。\n\n"
                                  "将跳转至浏览器下载安装包，\n"
                                  "下载完成后按默认选项安装即可。",
                                  QMessageBox::Yes | QMessageBox::No);

            if (reply == QMessageBox::Yes) {
                bool success = QDesktopServices::openUrl(QUrl("https://www.apple.com/itunes/download/win64"));
                if (!success)
                    QMessageBox::critical(this, "错误", "无法打开浏览器，请手动访问 https://www.apple.com/itunes/download/win64 下载");
            }
            
            return;
        }

        if (title == "兑换码") {
            RedeemRecordDialog(this).exec();
            return;
        }
        
        if (title == "续费") {
            RenewalDialog(this).exec();
            return;
        }

        if (title == "换绑") {
            SwapExpirationDialog(this).exec();
            return;
        }

        if (title == "软件更新") {
            if (QSysInfo::productType() != "windows") {
                new ToastWidget("苹果版本暂不支持此功能");
                return;
            }

            VersionManagerDialog(this).exec();
            return;
        }

        if (title == "客服") {
            showSupportDialog();
            return;
        }

        if (title == "日志") {
            LogTextBrowser::getInstance()->setParent(deviceListWidget);
            LogTextBrowser::getInstance()->toggleVisibility();
            return;
        }

        if (title == "开发者") {
            QMenu menu;
            menu.addAction("数据查看", [=]() {
                SettingsViewer dialog(settings, this);
                dialog.exec();
            });
            menu.addAction("可视化编程", [=]() {
                FlowEditorDialog(this).exec();
            });
            menu.addAction("兑换码生成", [this]() {
                QDialog dialog(this);
                dialog.setWindowTitle("生成兑换码");
                dialog.setMinimumWidth(300);

                QFormLayout *formLayout = new QFormLayout(&dialog);

                QLineEdit *phoneEdit = new QLineEdit(&dialog);
                phoneEdit->setPlaceholderText("请输入目标手机号");

                QSpinBox *countSpin = new QSpinBox(&dialog);
                countSpin->setRange(1, 1000);
                countSpin->setValue(1);

                formLayout->addRow("手机号:", phoneEdit);
                formLayout->addRow("生成数量:", countSpin);

                QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
                connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
                connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
                formLayout->addRow(buttonBox);

                if (dialog.exec() != QDialog::Accepted)
                    return;

                QString inputPhone = phoneEdit->text().trimmed();
                int inputCount = countSpin->value();

                webSocketClient->emitEvent("generate_codes", QJsonObject{{"phone", inputPhone}, {"count", inputCount}}, [=](const QJsonValue &res) {
                    if (res.isString()) {
                        QToolTip::showText(QCursor::pos(), res.toString());
                        return;
                    }

                    QStringList codes;
                    if (res.isArray()) {
                        for (const QJsonValue &item : res.toArray()) {
                            codes << item.toString(); 
                        }
                    }

                    qApp->clipboard()->setText(codes.join("\n"));
                    QToolTip::showText(QCursor::pos(), QString("成功生成 %1 个兑换码并复制").arg(codes.size()));
                });
            });
            menu.addAction("在线用户", [this]() {
                webSocketClient->emitEvent("online_accounts", QJsonValue(), [=](const QJsonValue &res) {
                    if (res.isString()) {
                        QToolTip::showText(QCursor::pos(), res.toString());
                        return;
                    }

                    QStringList phoneNumbers;
                    for (const QJsonValue &item : res.toArray()) {
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

    auto randomDelayChecked = settings->value("randomDelayChecked").toBool();
    randomDelayCheckBox->setChecked(randomDelayChecked);
    randomDelayCheckBox->hide();

    minDelaySpinBox->setRange(0, 99);
    maxDelaySpinBox->setRange(0, 99);
    minDelaySpinBox->setSuffix("秒");
    maxDelaySpinBox->setSuffix("秒");
    minDelaySpinBox->setValue(settings->value("minDelay", 3).toInt());
    maxDelaySpinBox->setValue(settings->value("maxDelay", 10).toInt());
    minDelaySpinBox->setMaximum(maxDelaySpinBox->value());
    maxDelaySpinBox->setMinimum(minDelaySpinBox->value());

    auto rangeLayout = new QHBoxLayout();
    rangeLayout->addWidget(minDelaySpinBox);
    rangeLayout->addWidget(new QLabel("—"));
    rangeLayout->addWidget(maxDelaySpinBox);
    Tools::setLayoutVisible(rangeLayout, multiControlSwitchButton->isChecked() && randomDelayChecked);

    connect(randomDelayCheckBox, &QCheckBox::clicked, [=](bool checked) {
        settings->setValue("randomDelayChecked", checked);
        Tools::setLayoutVisible(rangeLayout, checked);
    });

    connect(multiControlSwitchButton, &SwitchButton::toggled, [=](bool checked) {
        randomDelayCheckBox->setVisible(checked);
        Tools::setLayoutVisible(rangeLayout, checked && randomDelayCheckBox->isChecked());
    });

    connect(minDelaySpinBox, &QSpinBox::valueChanged, [this](int value) {
        maxDelaySpinBox->setMinimum(value);
        settings->setValue("minDelay", value);
        for (const auto& deviceWidget : getDeviceWidgets()) {
            deviceWidget->deviceInfo->randomDelay = 0;
        }
    });
    connect(maxDelaySpinBox, &QSpinBox::valueChanged, [this](int value) {
        minDelaySpinBox->setMaximum(value);
        settings->setValue("maxDelay", value);
        for (const auto& deviceWidget : getDeviceWidgets()) {
            deviceWidget->deviceInfo->randomDelay = 0;
        }
    });

    hLayout->addWidget(randomDelayCheckBox);
    hLayout->addLayout(rangeLayout);
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

    tabBar->installEventFilter(this);

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

        if (settings->value("sortSelectedToTop").toBool())
            deviceListWidget->sortItems(Qt::AscendingOrder);
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
        if (!data["springBoardMsg"].toString().isEmpty()) {
            qCriticalEx() << connection << data["springBoardMsg"];
            connection->close();
            return;
        }

        if (!data["choicyMsg"].toString().isEmpty()) {
            qCriticalEx() << connection << data["choicyMsg"];
            connection->close();
            return;
        }

        auto deviceInfo = DeviceInfo::getDevice(data["deviceId"].toString());

        if (deviceInfo) {
            if (deviceInfo->connection->type == connection->type) {
                if (deviceInfo->scanType)
                    connection->send("reject", "已存在相同连接");

                connection->close();
                return;
            }

            bool isUsbDevice = deviceInfo->connection->type == DeviceConnection::Usb;
            bool isUsbSetting = MainWindow::getInstance()->getTab().getConnectionMethod() == 0;

            if (isUsbDevice == isUsbSetting)
            {
                if (deviceInfo->scanType)
                    connection->send("reject", "已存在相同连接");
                
                connection->close();
                return;
            }
            else
            {
                deviceInfo->connection->close();
            }
        }

        deviceInfo = new DeviceInfo(connection, data.toObject());
        if (deviceInfo->isLockByOther()) {
            connection->send("reject", QString("此设备被【%1】独占，需要该账号退出独占模式您才能连接").arg(deviceInfo->getLocker()));
            
            connection->close();
            delete deviceInfo;
            return;
        }
        
        connection->deviceInfo = deviceInfo;
        addItem(connection);
    });

    EventHub::on(this, "disconnected", [this](const QJsonValue &data, DeviceConnection* connection) {
        qDebugEx() << "断开连接处理" << connection;

        if (!connection)
            return;

        for (auto listWidget : findChildren<ExplicitSelectionListWidget*>()) {
            for (int i = 0; i < listWidget->count(); i++) {
                const auto& item = listWidget->item(i);
                const auto& deviceWidget = item->data(Qt::UserRole).value<DeviceWidget*>();
                if (deviceWidget->connection == connection) {
                    listWidget->setEnabled(false);
                    delete listWidget->takeItem(i);
                    listWidget->setEnabled(true);
                    relayoutDevices();
                    return;
                }
            }
        }
    });

    webSocketClient->on("online_devices", [this](const QJsonValue &data, AckCallback callback) {
        auto& tab = getTab();
        const auto& devices = DeviceInfo::getDevices(1U << tab.bit);

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
            for (auto listWidget : findChildren<ExplicitSelectionListWidget*>()) {
                for (int i = 0; i < listWidget->count(); i++) {
                    const auto& item = listWidget->item(i);
                    const auto& deviceWidget = item->data(Qt::UserRole).value<DeviceWidget*>();
                    if (deviceWidget->deviceInfo->deviceId == udid) {
                        const auto& byteArray = deviceWidget->grabFrame();
                        callback(QJsonValue::fromVariant(byteArray.toBase64()));
                        return;
                    }
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

    webSocketClient->on("setDeviceLocker", [this](const QJsonValue &data) {
        const auto& udid = data[HIDE_STR("udid")].toString();
        const auto& locker = data["locker"].toString();

        const auto& deviceInfo = DeviceInfo::getDevice(udid);
        if (deviceInfo) {
            deviceInfo->setLocker(locker);

            if (locker == Account::getInstance()->phone)
                return;

            if (!locker.isEmpty())
                deviceInfo->connection->close();
        }
        else {
            DeviceInfo::setLocker(udid, locker);
        }
    });

    loadTabs();

    auto localIPs = NetworkUtils::getPhysicalIPs();
    qInfoEx() << "本机内网IP:" << localIPs;

    auto udpTransport = new UdpTransport(0, this);

    auto broadcastTask = [=]() {
        if (getTab().getAutoScanLANDevices() == 0)
            return;

        bool isUsbSetting = getTab().getConnectionMethod() == 0;
        const auto& ips = TcpServer::getInstance()->getConnectedIps();
        for (const auto& localIP : localIPs) {
            const auto& subnetIPs = NetworkUtils::getSubnetIPs(localIP);
            for (const auto& ip : std::as_const(subnetIPs)) {
                if (ips.contains(ip))
                    continue;

                if (DeviceInfo::isLockByOther(ip))
                    continue;

                const auto& deviceInfo = DeviceInfo::getDevice(ip);
                if (!deviceInfo || deviceInfo->connection->type == DeviceConnection::Usb && !isUsbSetting)
                    udpTransport->sendData(TcpServer::getInstance()->getHostInfo(localIP), ip, 32838);
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

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    auto tabBar = tabWidget->tabBar();
    if (watched != tabBar) {
        return QMainWindow::eventFilter(watched, event);
    }

    static int tabDragIndex = -1;
    static QPoint tabDragStartPos;

    if (event->type() == QEvent::MouseButtonPress) {
        auto me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            tabDragStartPos = me->pos();
            tabDragIndex = tabBar->tabAt(tabDragStartPos);
        }
        return false;
    }

    if (event->type() == QEvent::MouseButtonRelease) {
        tabDragIndex = -1;
        return false;
    }

    if (event->type() == QEvent::MouseMove) {
        auto me = static_cast<QMouseEvent*>(event);
        
        if (!(me->buttons() & Qt::LeftButton) || tabDragIndex == -1) return false;
        if ((me->pos() - tabDragStartPos).manhattanLength() <= QApplication::startDragDistance()) return false;
        if (tabBar->rect().contains(me->pos())) return false; // 未拖出标签栏
        if (tabWidget->count() <= 1) { tabDragIndex = -1; return false; } // 仅剩最后一个标签，禁止拖出

        int indexToTear = tabDragIndex;
        tabDragIndex = -1; // 触发后立即复位

        // 释放标签栏的鼠标占用，防止原生控件卡死
        QMouseEvent cancelEvent(QEvent::MouseButtonRelease, me->pos(), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(tabBar, &cancelEvent);

        // 1. 创建悬浮窗口
        auto tabItem = tabs.takeAt(indexToTear);
        auto fw = new FloatingTabWindow(this, tabItem);

        QSize contentSize = tabWidget->currentWidget()->size();
        fw->resize(contentSize.isEmpty() ? tabWidget->size() : contentSize);
        QPoint globalPos = tabBar->mapToGlobal(me->pos());
        fw->move(globalPos.x() - 10, globalPos.y() - 10);

        // ==========================================
        // 处理设备在不同 ListWidget 间的无损转移
        // ==========================================
        auto moveDevices = [this](QListWidget* src, QListWidget* dst, uint32_t mask) {
            // 屏蔽两个列表的 selectionModel 信号，防止转移过程中误触列表的“取消选中”逻辑
            const QSignalBlocker srcBlocker(src->selectionModel());
            const QSignalBlocker dstBlocker(dst->selectionModel());

            for (int i = src->count() - 1; i >= 0; --i) {
                auto player = src->item(i)->data(Qt::UserRole).value<DeviceWidget*>();
                if (mask == 0 || (player->deviceInfo->groupMask & mask)) {
                    bool wasChecked = player->checkBox->isChecked();

                    player->setParent(nullptr); 
                    
                    auto item = src->takeItem(i); 

                    auto frame = new QFrame();
                    frame->setFrameShape(QFrame::Box);
                    auto layout = new QVBoxLayout(frame);
                    layout->setContentsMargins(0, 0, 0, 0);
                    layout->addWidget(player);

                    dst->addItem(item);
                    dst->setItemWidget(item, frame);
                    
                    item->setSelected(wasChecked);
                }
            }
            
            // 转移完成后根据设置项重新排序
            if (settings->value("sortSelectedToTop").toBool()) {
                dst->sortItems(Qt::AscendingOrder);
            }
        };

        // 2. 同步悬浮窗的信号与逻辑
        connect(fw->listWidget, &QListWidget::itemPressed, [fw](QListWidgetItem *item) {
            if (auto widget = fw->listWidget->itemWidget(item)) 
                widget->findChild<DeviceWidget*>()->setFocus();
        });
        
        connect(fw->listWidget->selectionModel(), &QItemSelectionModel::selectionChanged, this, [=](const QItemSelection &sel, const QItemSelection &desel) {
            auto updateCheck = [](const QModelIndexList& list, bool checked) {
                for (const auto& idx : list) {
                    if (auto player = idx.data(Qt::UserRole).value<DeviceWidget*>()) {
                        const QSignalBlocker blocker(player->checkBox);
                        player->checkBox->setChecked(checked);
                    }
                }
            };
            updateCheck(sel.indexes(), true);
            updateCheck(desel.indexes(), false);
            if (settings->value("sortSelectedToTop").toBool()) fw->listWidget->sortItems(Qt::AscendingOrder);
        });

        connect(fw, &FloatingTabWindow::closed, this, [=, this]() {
            moveDevices(fw->listWidget, deviceListWidget, 0); // 无损归还主窗口
            tabs.append(fw->tabItem);
            tabWidget->addTab(new QWidget(), fw->tabItem.name);
            saveTabs();
            relayoutDevices();
            fw->deleteLater();
        });

        // 3. 执行物理转移并剥离原标签
        moveDevices(deviceListWidget, fw->listWidget, 1U << tabItem.bit);

        auto page = tabWidget->widget(indexToTear);
        if (deviceListWidget->parentWidget() == page) {
            deviceListWidget->setParent(this);
        }
        tabWidget->removeTab(indexToTear);
        page->deleteLater();
        saveTabs();
        
        // 4. 显示并接管系统原生拖拽
        fw->show();
        relayoutDevices();
        if (fw->windowHandle()) {
            fw->windowHandle()->startSystemMove();
        }

        return true;
    }

    return QMainWindow::eventFilter(watched, event); 
}

void MainWindow::showSupportDialog()
{
    if (!QFile::exists(qApp->applicationDirPath() + "/imageformats/qpng.dll")) {
        QToolTip::showText(QCursor::pos(), "请联系客服充值");
        return;
    }

    auto supportDialog = new QDialog(this);
    supportDialog->setAttribute(Qt::WA_DeleteOnClose);
    supportDialog->setWindowTitle("客服");

    auto mainLayout = new QHBoxLayout(supportDialog);
    mainLayout->setSizeConstraint(QLayout::SetFixedSize);

    auto pixmap = QPixmap(qApp->applicationDirPath() + "/imageformats/qpng.dll").scaled(
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

int MainWindow::getRandomDelay()
{
    if (!multiControlSwitchButton->isChecked())
        return 0;

    if (!randomDelayCheckBox->isChecked())
        return 0;

    int min = minDelaySpinBox->value() * 1000;
    int max = maxDelaySpinBox->value() * 1000;
    return min + QRandomGenerator::global()->generateDouble() * (max - min);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!isMinimized())
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

    if (event->matches(QKeySequence::FullScreen)) {
        isFullScreen() ? showNormal() : showFullScreen();
        return;
    }

    if (event->key() == Qt::Key_Escape) {
        if (isFullScreen()) {
            showNormal();
            return;
        }
        
        auto reply = QMessageBox::question(this, "确认退出", "你确定要关闭吗？",
                                  QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes)
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
    for (const auto& deviceWidget : getDeviceWidgets()) {
        auto videoFrameWidget = deviceWidget->getDeviceWindow() ? deviceWidget->getDeviceWindow()->getVideoFrameWidget() : deviceWidget->getVideoFrameWidget();
        const auto& size = videoFrameWidget->size();
        qApp->postEvent(videoFrameWidget, new QResizeEvent(size, size));
    }
}

void MainWindow::relayoutDevices()
{
    // 计算目标尺寸并应用到指定的 QListWidget 中，同时返回该分组内的设备总数
    auto applyLayout = [&](BitMaskEditorDialog::Item tabItem, QListWidget* listWidget) -> int {
        const auto scale = tabItem.scale == 0 ? 1 : tabItem.scale / 100.0f;
        const auto isLandscape = tabItem.getIsLandscape();
        
        const auto& devicesInGroup = DeviceInfo::getDevices(1U << tabItem.bit);
        
        auto frameItemHeight = frameItemWidth * DeviceInfo::getOptimalAspectRatio(devicesInGroup);
        int targetW = (isLandscape ? frameItemHeight : frameItemWidth) * scale;
        int targetH = (isLandscape ? frameItemWidth : frameItemHeight) * scale;
        QSize targetSize(targetW, targetH);

        int actualVisibleCount = 0;

        for (int i = 0; i < listWidget->count(); ++i) {
            const auto& item = listWidget->item(i);
            const auto& deviceWidget = item->data(Qt::UserRole).value<DeviceWidget*>();

            item->setHidden(!deviceWidget || !devicesInGroup.contains(deviceWidget->deviceInfo));

            if (!item->isHidden()) {
                item->setSizeHint(targetSize + QSize(0, 46));
                deviceWidget->setFixedSize(targetSize + QSize(0, 46));
                actualVisibleCount++; // 只有未隐藏且物理存在于该容器的才计数
            }
        }
        return actualVisibleCount;
    };

    // ==========================================
    // 1. 处理主窗口的设备布局和标签栏状态
    // ==========================================
    if (tabWidget->count() > 0 && !tabs.isEmpty()) {
        applyLayout(getTab(), deviceListWidget);

        // 刷新所有 Tab 的角标，只统计当前物理上还在deviceListWidget的设备，排除已拖走的
        for (int i = 0; i < tabWidget->count(); ++i) {
            const auto& t = tabs[i];
            int count = 0;
            for (int j = 0; j < deviceListWidget->count(); ++j) {
                const auto& item = deviceListWidget->item(j);
                const auto& deviceWidget = item->data(Qt::UserRole).value<DeviceWidget*>();
                // 判断此卡片是否属于这个分组
                if (deviceWidget && (deviceWidget->deviceInfo->groupMask & (1U << t.bit))) {
                    count++;
                }
            }
            tabWidget->setTabText(i, QString("%1 [%2]").arg(t.name).arg(count));
        }
    }

    // ==========================================
    // 2. 处理所有独立悬浮窗口的设备布局和标题状态
    // ==========================================
    for (auto fw : this->findChildren<FloatingTabWindow*>()) {
        int count = applyLayout(fw->tabItem, fw->listWidget);
        fw->setWindowTitle(QString("%1 [%2]").arg(fw->tabItem.name).arg(count));
    }

    // videoVisibilityManager->refresh();
}

void MainWindow::addItem(DeviceConnection* connection)
{
    connection->send("server", QJsonObject{{"accountId", Account::getInstance()->id}, {"ip", Config::SERVER_IP}, {"port", Config::SERVER_PORT}});

    auto deviceInfo = connection->deviceInfo;

    auto player = new DeviceWidget(connection, deviceInfo);

    deviceInfo->expireAt = DeviceInfo::expirations.value(deviceInfo->deviceId);
    if (deviceInfo->expireAt.get() == 0)
    {
        auto retryTimer = new QTimer(player);
        
        connect(retryTimer, &QTimer::timeout, player, [=]() {
            retryTimer->setInterval(3000);

            if (deviceInfo->expireAt.get() != 0) {
                retryTimer->deleteLater();
                return;
            }

            webSocketClient->emitEvent("deviceInfo", QJsonObject{{HIDE_STR("udid"), deviceInfo->deviceId}, {"isUsb", connection->type == DeviceConnection::Usb}}, [=](const QJsonValue &res) {
                deviceInfo->expireAt = res[HIDE_STR("expireAt")].toInteger();
                DeviceInfo::expirations[deviceInfo->deviceId] = deviceInfo->expireAt;

                deviceInfo->setLocker(res["locker"].toString());
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

            qint64 expireTime = deviceInfo->expireAt.get();
            qint64 currentTime = Account::getInstance()->loginTime.get() + elapsedTimer->elapsed();
            if (expireTime > currentTime)
            {
                device->appendData(data);

                if (expireTime - currentTime < HIDE_NUM(86400000))
                    ipLabel->setText(deviceInfo->localIp + HIDE_STR("<font color='orange'>[即将过期]</font>"));
                else
                    ipLabel->setText(deviceInfo->localIp);
            }
            else
            {
                ipLabel->setText(deviceInfo->localIp + (deviceInfo->expireAt.get() == 0 ? "" : HIDE_STR("<font color='red'>[已过期]</font>")));
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

                qint64 expireTime = deviceInfo->expireAt.get();
                qint64 currentTime = Account::getInstance()->loginTime.get() + elapsedTimer->elapsed();
                if (expireTime > currentTime)
                {
                    device->appendData(data);

                    if (expireTime - currentTime < HIDE_NUM(86400000))
                        ipLabel->setText(deviceInfo->localIp + HIDE_STR("<font color='orange'>[即将过期]</font>"));
                    else
                        ipLabel->setText(deviceInfo->localIp);
                }
                else
                {
                    ipLabel->setText(deviceInfo->localIp + (deviceInfo->expireAt.get() == 0 ? "" : HIDE_STR("<font color='red'>[已过期]</font>")));
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

    auto item = new NaturalSortListWidgetItem();
    item->setText(deviceInfo->deviceName);
    item->setData(Qt::UserRole, QVariant::fromValue(player));

    // 根据所属组动态分配到对应的 ListWidget（解决刚连接的设备自动掉进悬浮窗）
    auto targetList = deviceListWidget;
    for (auto fw : this->findChildren<FloatingTabWindow*>()) {
        if (deviceInfo->groupMask & (1U << fw->tabItem.bit)) {
            targetList = fw->listWidget;
            break;
        }
    }

    connect(player->checkBox, &QCheckBox::stateChanged, [=](int state) {
        auto currentList = item->listWidget();
        if (currentList) {
            const QSignalBlocker blocker(currentList);
            item->setSelected(state == Qt::Checked);

            if (settings->value("sortSelectedToTop").toBool())
                currentList->sortItems(Qt::AscendingOrder);
        }

        bool isUserAction = player->checkBox->hasFocus() || player->checkBox->isDown();
        if (!isUserAction)
            return;

        QToolTip::showText(QCursor::pos(), R"(
            <div>
                <b>快捷操作指南：</b>
                <ul style='margin-left: -15px; margin-top: 5px;'>
                    <li>按住 <b>Ctrl + 点击</b>：切换选中</li>
                    <li>空白处 <b>Ctrl + A</b>：全选</li>
                    <li>点击空白处：全不选</li>
                </ul>
            </div>
        )");
    });

    targetList->addItem(item);
    targetList->setItemWidget(item, frame);
    item->setSelected(true);

    player->setProperty("listWidgetItem", QVariant::fromValue(static_cast<QListWidgetItem*>(item)));

    relayoutDevices();
}

QList<DeviceConnection*> MainWindow::getDeviceConnections(DeviceView* mainDeviceView)
{
    QList<DeviceConnection*> list;
 
    for (const auto& widget : getDeviceWidgets(mainDeviceView)) {
        list.append(widget->connection);
    }

    return list;
}

QList<DeviceWidget*> MainWindow::getDeviceWidgets(DeviceView* mainDeviceView)
{
    QListWidget* targetListWidget = deviceListWidget;

    if (mainDeviceView) {
        auto deviceWidget = qobject_cast<DeviceWidget*>(mainDeviceView);
        if (!deviceWidget)
            deviceWidget = qobject_cast<DeviceWindow*>(mainDeviceView)->deviceWidget;

        // 如果未开启群控，或当前设备不可控，直接返回该设备
        if (!multiControlSwitchButton->isChecked() || !deviceWidget->deviceInfo->controller) 
            return { deviceWidget };

        // 提取绑定的 QListWidgetItem，获取它当前真实所在的 QListWidget（主窗口或悬浮窗）
        targetListWidget = deviceWidget->property("listWidgetItem").value<QListWidgetItem*>()->listWidget();

        // 如果当前点击操作的设备没有勾选，说明用户只是想单独控制它，不触发同屏群控
        // if (!deviceWidget->checkBox->isChecked())
        //     return { deviceWidget };
    }

    QList<DeviceWidget*> list;

    // 只在当前设备关联的特定窗口（分组）中寻找勾选的设备
    for (int i = 0; i < targetListWidget->count(); i++) {
        const auto& item = targetListWidget->item(i);
        if (item->isHidden())
            continue;

        const auto& widget = item->data(Qt::UserRole).value<DeviceWidget*>();
        
        if (widget->checkBox->isChecked())
            list.append(widget);
    }

    return list;
}

QList<DeviceWindow*> MainWindow::getDeviceWindows(DeviceView* mainDeviceView)
{
    QList<DeviceWindow*> list;
 
    for (const auto& widget : getDeviceWidgets(mainDeviceView)) {
        if (widget->getDeviceWindow())
            list.append(widget->getDeviceWindow());
    }

    return list;
}

QList<QString> MainWindow::getDeviceUdids(DeviceView* mainDeviceView)
{
    QList<QString> list;
 
    for (const auto& widget : getDeviceWidgets(mainDeviceView)) {
        list.append(widget->deviceInfo->deviceId);
    }

    return list;
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

    QMenu menu;

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
                for (auto& deviceInfo : DeviceInfo::getDevices(mask)) {
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
            QStringList list = {"默认", "", "低清", "标清", "高清", "超清"};
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

#include "MainWindow.moc"
