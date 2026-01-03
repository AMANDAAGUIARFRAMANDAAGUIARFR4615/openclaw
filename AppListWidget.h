#pragma once

#include "DeviceConnection.h"
#include "EventHub.h"
#include "RemoteFileExplorer.h"
#include "ToastWidget.h"
#include "MainWindow.h"
#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QStyleOption>
#include <QPainter>
#include <QJsonArray>
#include <QPainterPath>
#include <QMessageBox>
#include <QKeyEvent>
#include <QLineEdit>

class AppListWidget : public QWidget
{
    Q_OBJECT

public:
    static AppListWidget* open(DeviceConnection* connection) {
        auto existing = instanceMap.value(connection);
        if (existing) {
            existing->setWindowState(existing->windowState() & ~Qt::WindowMinimized);
            existing->raise();
            existing->activateWindow();
            return existing;
        }

        connection->send("appList");

        AppListWidget *appList = new AppListWidget(connection);
        appList->setWindowTitle(connection->displayName() + " - 应用管理");
        appList->resize(1080, 720);
        appList->show();
        return appList;
    }

private:
    explicit AppListWidget(DeviceConnection* connection) : connection(connection), QWidget() {
        instanceMap[connection] = this;

        setAttribute(Qt::WA_DeleteOnClose);
        // 设置整体背景色为白色，避免灰底
        this->setStyleSheet("background-color: white;");

        QLineEdit *searchEdit = new QLineEdit(this);
        searchEdit->setPlaceholderText("🔍 搜索应用名或包名...");
        searchEdit->setClearButtonEnabled(true);
        // 搜索框美化：圆角、内边距、聚焦边框颜色
        searchEdit->setStyleSheet(R"(
            QLineEdit {
                padding: 8px 15px;
                border: 1px solid #E0E0E0;
                border-radius: 18px;
                background-color: #F5F7FA;
                font-size: 14px;
                color: #333;
                selection-background-color: #007AFF;
            }
            QLineEdit:focus {
                border: 1px solid #007AFF;
                background-color: #FFFFFF;
            }
        )");

        table = new QTableWidget(this);
        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(20, 20, 20, 20);
        mainLayout->setSpacing(15);
        mainLayout->addWidget(searchEdit);
        mainLayout->addWidget(table);
        setLayout(mainLayout);

        setupTable();
        applyStyle();

        connect(searchEdit, &QLineEdit::textChanged, [this](const QString &text) {
            QString query = text.trimmed().toLower();
            for (int i = 0; i < table->rowCount(); ++i) {
                bool hidden = true;
                // 获取应用名 (列1) 和 包名 (列2)
                QTableWidgetItem *nameItem = table->item(i, 1);
                QTableWidgetItem *pkgItem = table->item(i, 2);

                if (query.isEmpty()) {
                    hidden = false;
                } else {
                    if (nameItem && nameItem->text().toLower().contains(query)) hidden = false;
                    else if (pkgItem && pkgItem->text().toLower().contains(query)) hidden = false;
                }
                table->setRowHidden(i, hidden);
            }
        });

        EventHub::on(this, "appList", [this](const QJsonValue &data, DeviceConnection* connection) {
            if (this->connection != connection)
                return;

            table->setSortingEnabled(false);
            table->setRowCount(0);

            QJsonArray appArray = data.toArray();
            for (const QJsonValue &item : appArray) {
                if (!item.isObject())
                    continue;

                addApp(item.toObject());
            }
            table->setSortingEnabled(true);
        });

        EventHub::on(this, "appOperation", [this](const QJsonValue &data, DeviceConnection* connection) {
            if (this->connection != connection)
                return;

            auto name = data["name"].toString();
            auto path = data["path"].toString();

            if (path == "")
            {
                new ToastWidget("路径不存在或无法访问", this);
                return;
            }

            RemoteFileExplorer::open(connection, path);
        });
    }

    ~AppListWidget() {
        EventHub::off(this, "appList");
        EventHub::off(this, "appOperation");
        instanceMap.remove(connection);
    }

    void addApp(const QJsonObject &jsonObject) {
        const auto& iconBase64 = jsonObject["icon"].toString();
        const auto& appName = jsonObject["name"].toString();
        const auto& packageName = jsonObject["identifier"].toString();
        const auto& type = jsonObject["type"].toInt();

        int row = table->rowCount();
        table->insertRow(row);
        table->setRowHeight(row, 60);

        // 图标
        QLabel *iconLabel = new QLabel();
        QByteArray byteArray = QByteArray::fromBase64(iconBase64.toUtf8());
        QPixmap pix;
        if (!pix.loadFromData(byteArray)) {
            qCriticalEx() << "图标加载失败";
        }

        int rowHeight = table->rowHeight(row);
        int iconBoxSize = 48;

        QPixmap scaled = pix.scaled(iconBoxSize, iconBoxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        // 圆角裁剪图标
        QPixmap rounded(iconBoxSize, iconBoxSize);
        rounded.fill(Qt::transparent);
        QPainter painter(&rounded);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

        QPainterPath path;
        path.addRoundedRect(0, 0, iconBoxSize, iconBoxSize, 10, 10); // iOS风格圆角
        painter.setClipPath(path);
        painter.drawPixmap(0, 0, scaled);

        iconLabel->setAttribute(Qt::WA_TranslucentBackground);
        iconLabel->setPixmap(rounded);
        iconLabel->setAlignment(Qt::AlignCenter);
        
        // 为了让图标居中，包裹在一个Widget里
        QWidget* iconContainer = new QWidget();
        QHBoxLayout* iconLayout = new QHBoxLayout(iconContainer);
        iconLayout->setContentsMargins(0,0,0,0);
        iconLayout->setAlignment(Qt::AlignCenter);
        iconLayout->addWidget(iconLabel);
        table->setCellWidget(row, 0, iconContainer);

        // 应用名
        QTableWidgetItem *appItem = new QTableWidgetItem(appName);
        appItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        appItem->setFont(QFont("Microsoft YaHei", 10, QFont::Bold)); // 加粗应用名
        table->setItem(row, 1, appItem);

        // 包名
        QTableWidgetItem *pkgItem = new QTableWidgetItem(packageName);
        pkgItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        pkgItem->setForeground(QBrush(QColor("#666666"))); // 包名颜色淡一点
        table->setItem(row, 2, pkgItem);

        // 操作按钮区域
        QWidget *actionWidget = new QWidget();
        QHBoxLayout *layout = new QHBoxLayout(actionWidget);
        layout->setContentsMargins(5, 5, 5, 5);
        layout->setSpacing(8); // 按钮间距
        layout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        layout->setSizeConstraint(QLayout::SetFixedSize);

        QStringList btnNames = {
            "卸载",
            "沙盒路径",
            "安装路径",
            "共享路径",
            "清缓存",
            "清钥匙串",
            "打开",
            "关闭"
        };

        static const QSet<QString> forbiddenNames = {"卸载", "清缓存", "清钥匙串"};

        for (int i = 0; i < btnNames.size(); ++i) {
            const auto &name = btnNames[i];
            auto button = new QPushButton(name);
            button->setCursor(Qt::PointingHandCursor);
            
            // 基础样式
            QString baseStyle = R"(
                QPushButton {
                    border-radius: 4px;
                    padding: 5px 8px;
                    font-size: 12px;
                    font-family: "Microsoft YaHei";
                    border: 1px solid transparent;
                    min-width: 50px; /* [修改点] 增加最小宽度 */
                }
                QPushButton:pressed {
                    padding-top: 6px; 
                    padding-bottom: 4px;
                }
            )";

            // 针对不同按钮类型的特定样式
            QString specificStyle;
            if (name == "卸载") {
                // 红色警告样式
                specificStyle = R"(
                    QPushButton {
                        background-color: #FFF1F0;
                        color: #FF4D4F;
                        border: 1px solid #FFCCC7;
                    }
                    QPushButton:hover {
                        background-color: #FF4D4F;
                        color: white;
                        border: 1px solid #FF4D4F;
                    }
                )";
            } else {
                // 默认蓝色/灰色样式
                specificStyle = R"(
                    QPushButton {
                        background-color: #F0F5FF;
                        color: #2F54EB;
                        border: 1px solid #ADC6FF;
                    }
                    QPushButton:hover {
                        background-color: #2F54EB;
                        color: white;
                        border: 1px solid #2F54EB;
                    }
                )";
            }

            QString disabledStyle = R"(
                QPushButton:disabled {
                    background-color: #F5F5F5;
                    color: #BDBDBD;
                    border: 1px solid #E0E0E0;
                }
            )";

            button->setStyleSheet(baseStyle + specificStyle + disabledStyle);

            layout->addWidget(button);

            if (type != 1 && forbiddenNames.contains(name)) {
                button->setEnabled(false);
                button->setCursor(Qt::ForbiddenCursor); // 禁用时鼠标变禁止符号
            }

            connect(button, &QPushButton::clicked, [=](bool) {
                bool needConfirm = forbiddenNames.contains(name);
                if (needConfirm) {
                    QMessageBox msgBox(this);
                    msgBox.setWindowTitle("确认操作");
                    msgBox.setText(QString("确定要执行“%1”操作吗？\n此操作不可撤销。").arg(name));
                    msgBox.setIcon(QMessageBox::Warning);
                    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
                    msgBox.setDefaultButton(QMessageBox::No);
                    // 简单美化MessageBox
                    msgBox.setStyleSheet("QLabel{min-width: 200px;}"); 
                    if (msgBox.exec() != QMessageBox::Yes)
                        return;
                }

                QJsonObject dataObject;
                dataObject["identifier"] = packageName;
                dataObject["name"] = name;
                dataObject["type"] = i + 1;

                if (!name.endsWith("路径") && MainWindow::getInstance()->multiControlSwitchButton->isChecked())
                {
                    const auto& devices = MainWindow::getInstance()->getDevices();
                    for(const auto& device : std::as_const(devices)) {
                        device->connection->send("appOperation", dataObject);
                    }
                }
                else
                {
                    connection->send("appOperation", dataObject);
                }

                if (name == "卸载")
                {
                    int currentRow = table->indexAt(button->parentWidget()->pos()).row();
                    if (currentRow >= 0)
                        table->removeRow(currentRow);
                }
            });
        }
        
        layout->addStretch();
        
        actionWidget->setLayout(layout);
        table->setCellWidget(row, 3, actionWidget);
    }

protected:
    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_Escape)
            close();
        else
            QWidget::keyPressEvent(event);
    }

private:
    void setupTable() {
        table->setColumnCount(4);
        table->setHorizontalHeaderLabels(QStringList() << "图标" << "应用名称" << "包名" << "操作");

        table->verticalHeader()->setVisible(false);
        // 去除虚线框
        table->setFocusPolicy(Qt::NoFocus); 
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        
        // 不显示网格线，改用StyleSheet做分割线
        table->setShowGrid(false); 
        
        // 开启交替行背景
        table->setAlternatingRowColors(true);

        // 设置列宽
        table->setColumnWidth(0, 80);  // 图标列宽一点
        table->setColumnWidth(1, 200); // 应用名
        table->setColumnWidth(2, 250); // 包名

        table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
        table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    }

    void applyStyle() {
        // 全局样式表：表格、表头、滚动条
        QString qss = R"(
            QWidget {
                font-family: "Microsoft YaHei", "Segoe UI", sans-serif;
                font-size: 13px;
                color: #333333;
            }
            
            /* 表格整体样式 */
            QTableWidget {
                background-color: #FFFFFF;
                border: 1px solid #E0E0E0;
                border-radius: 8px;
                gridline-color: #F0F0F0;
                outline: none; /* 去除选中时的虚线框 */
            }
            
            /* 表格项样式 */
            QTableWidget::item {
                padding: 5px;
                border-bottom: 1px solid #F5F5F5; /* 自定义行底部分割线 */
            }
            
            /* 选中行样式 */
            QTableWidget::item:selected {
                background-color: #E6F7FF; /* 选中变为非常淡的蓝色 */
                color: #333333; /* 文字颜色不变 */
            }
            
            /* 表头样式 */
            QHeaderView::section {
                background-color: #FAFAFA;
                color: #666666;
                padding: 10px 5px;
                border: none;
                border-bottom: 2px solid #EEEEEE;
                font-weight: bold;
                font-size: 13px;
            }
            
            /* 垂直滚动条美化 */
            QScrollBar:vertical {
                border: none;
                background: #F5F5F5;
                width: 8px;
                margin: 0px 0px 0px 0px;
                border-radius: 4px;
            }
            QScrollBar::handle:vertical {
                background: #CCCCCC;
                min-height: 20px;
                border-radius: 4px;
            }
            QScrollBar::handle:vertical:hover {
                background: #999999;
            }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
                height: 0px;
            }
        )";
        
        this->setStyleSheet(qss);
    }

    DeviceConnection* const connection;
    QTableWidget *table;

    inline static QMap<DeviceConnection*, AppListWidget*> instanceMap;
};