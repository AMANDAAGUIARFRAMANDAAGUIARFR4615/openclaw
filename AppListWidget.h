#pragma once

#include "DeviceConnection.h"
#include "EventHub.h"
#include "RemoteFileExplorer.h"
#include "ToastWidget.h"
#include "MainWindow.h"
#include "DeviceView.h"
#include <QDialog>
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
#include <QRadioButton>
#include <QButtonGroup>

class AppListWidget : public QDialog
{
    Q_OBJECT

public:
    explicit AppListWidget(DeviceConnection* connection, DeviceView *parent) : connection(connection), QDialog(parent) {
        setAttribute(Qt::WA_DeleteOnClose);
        setWindowModality(Qt::WindowModal);
        open();

        setWindowTitle(connection->deviceInfo->deviceName + "[应用管理]");
        resize(1080, 720);
        connection->send("appList");
        
        QLineEdit *searchEdit = new QLineEdit(this);
        searchEdit->setPlaceholderText("🔍 搜索应用名或包名...");
        searchEdit->setClearButtonEnabled(true);
        searchEdit->setStyleSheet(R"(
            QLineEdit {
                padding: 6px 10px;
                border: 1px solid palette(mid);
                border-radius: 4px;
                background-color: palette(base);
                color: palette(text);
                selection-background-color: palette(highlight);
                selection-color: palette(highlighted-text);
            }
            QLineEdit:focus {
                border: 1px solid palette(highlight);
            }
        )");

        QWidget *filterContainer = new QWidget(this);
        QHBoxLayout *filterLayout = new QHBoxLayout(filterContainer);
        filterLayout->setContentsMargins(10, 0, 10, 0);
        filterLayout->setSpacing(20);

        QLabel *filterLabel = new QLabel("应用类型:", this);
        // 稍微加粗标签
        QFont labelFont = filterLabel->font();
        labelFont.setBold(true);
        filterLabel->setFont(labelFont);

        QRadioButton *rbAll = new QRadioButton("全部", this);
        QRadioButton *rbUser = new QRadioButton("用户", this);
        QRadioButton *rbSystem = new QRadioButton("系统", this);

        rbAll->setChecked(true); // 默认全选

        QButtonGroup *filterGroup = new QButtonGroup(this);
        filterGroup->addButton(rbAll, 0);    // ID 0: 全部
        filterGroup->addButton(rbUser, 1);   // ID 1: 用户
        filterGroup->addButton(rbSystem, 2); // ID 2: 系统

        filterLayout->addWidget(filterLabel);
        filterLayout->addWidget(rbAll);
        filterLayout->addWidget(rbUser);
        filterLayout->addWidget(rbSystem);
        filterLayout->addStretch();

        table = new QTableWidget(this);
        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(15, 15, 15, 15);
        mainLayout->setSpacing(12);
        mainLayout->addWidget(searchEdit);
        mainLayout->addWidget(filterContainer);
        mainLayout->addWidget(table);
        setLayout(mainLayout);

        setupTable();

        // 统一的过滤逻辑函数
        auto performFilter = [this, searchEdit, filterGroup]() {
            QString query = searchEdit->text().trimmed().toLower();
            int filterType = filterGroup->checkedId(); // 0=全部, 1=用户, 2=系统

            for (int i = 0; i < table->rowCount(); ++i) {
                bool matchText = true;
                bool matchType = true;

                // 1. 文本过滤
                if (!query.isEmpty()) {
                    QTableWidgetItem *nameItem = table->item(i, 1);
                    QTableWidgetItem *pkgItem = table->item(i, 2);
                    bool nameContains = nameItem && nameItem->text().toLower().contains(query);
                    bool pkgContains = pkgItem && pkgItem->text().toLower().contains(query);
                    if (!nameContains && !pkgContains)
                        matchText = false;
                }

                // 2. 类型过滤
                if (filterType != 0) { // 如果不是选“全部”
                    int appType = table->item(i, 1)->data(Qt::UserRole).toInt();
                    if (appType != filterType)
                        matchType = false;
                }

                table->setRowHidden(i, !(matchText && matchType));
            }
        };

        connect(searchEdit, &QLineEdit::textChanged, performFilter);
        connect(filterGroup, &QButtonGroup::idClicked, performFilter);

        EventHub::on(this, "appList", [this](const QJsonValue &data, DeviceConnection* connection) {
            if (this->connection != connection)
                return;

            table->setSortingEnabled(false);
            table->setRowCount(0);

            for (const QJsonValue &item : data.toArray()) {
                if (!item.isObject())
                    continue;

                addApp(item.toObject());
            }
            table->setSortingEnabled(true);
        });

        EventHub::on(this, "appOperation", [=](const QJsonValue &data, DeviceConnection* connection) {
            if (this->connection != connection)
                return;

            auto name = data["name"].toString();
            auto path = data["path"].toString();

            if (path == "")
            {
                new ToastWidget("路径不存在或无法访问", this);
                return;
            }

            new RemoteFileExplorer(connection, path, parent);
        });
    }

    ~AppListWidget() {
        EventHub::off(this, "appList");
        EventHub::off(this, "appOperation");
    }

protected:

    void addApp(const QJsonObject &jsonObject) {
        const auto& iconBase64 = jsonObject["icon"].toString();
        const auto& appName = jsonObject["name"].toString();
        const auto& packageName = jsonObject["identifier"].toString();
        const auto& type = jsonObject["type"].toInt();
        const auto& isDeletable = jsonObject["isDeletable"].toBool();

        int row = table->rowCount();
        table->insertRow(row);
        table->setRowHeight(row, 56);

        // 图标
        QLabel *iconLabel = new QLabel();
        QByteArray byteArray = QByteArray::fromBase64(iconBase64.toUtf8());
        QPixmap pix;
        if (!pix.loadFromData(byteArray)) {
            qCriticalEx() << "图标加载失败";
        }

        int iconBoxSize = 40;

        QPixmap scaled = pix.scaled(iconBoxSize, iconBoxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        // 圆角裁剪图标
        QPixmap rounded(iconBoxSize, iconBoxSize);
        rounded.fill(Qt::transparent);
        QPainter painter(&rounded);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

        QPainterPath path;
        path.addRoundedRect(0, 0, iconBoxSize, iconBoxSize, 8, 8); // 圆角半径
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
        // 使用系统字体，但保留加粗
        QFont nameFont = appItem->font();
        nameFont.setBold(true);
        nameFont.setPointSize(10);
        appItem->setFont(nameFont);
        appItem->setData(Qt::UserRole, type); 
        table->setItem(row, 1, appItem);

        // 包名
        QTableWidgetItem *pkgItem = new QTableWidgetItem(packageName);
        pkgItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        pkgItem->setForeground(QBrush(QColor("#606060"))); // 标准灰色
        table->setItem(row, 2, pkgItem);

        // 操作按钮区域
        QWidget *actionWidget = new QWidget();
        QHBoxLayout *layout = new QHBoxLayout(actionWidget);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(6); 
        layout->setAlignment(Qt::AlignCenter);

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
            
            // 基础样式
            QString baseStyle = R"(
                QPushButton {
                    border: 1px solid palette(mid);
                    border-radius: 3px;
                    background-color: palette(button);
                    padding: 4px 10px;
                    min-width: 40px;
                    color: palette(button-text);
                }
                QPushButton:hover {
                    background-color: palette(light);
                    border: 1px solid palette(dark);
                }
                QPushButton:pressed {
                    background-color: palette(midlight);
                    border: 1px solid palette(shadow);
                }
                QPushButton:disabled {
                    background-color: palette(window);
                    color: palette(disabled(text));
                    border: 1px solid palette(midlight);
                }
            )";

            // 特殊按钮样式
            QString specificStyle;
            if (name == "卸载") {
                // 红色文字，边框微红
                specificStyle = R"(
                    QPushButton {
                        color: #EF5350; 
                        border: 1px solid #EF5350;
                    }
                    QPushButton:hover {
                        border: 1px solid #D32F2F;
                        background-color: rgba(211, 47, 47, 40); 
                    }
                )";
            } 

            button->setStyleSheet(baseStyle + specificStyle);

            layout->addWidget(button);

            if (type != 1 && forbiddenNames.contains(name)) {
                auto enabled = name == "卸载" ? isDeletable : false;
                button->setEnabled(enabled);
                if (!enabled)
                    button->setCursor(Qt::ForbiddenCursor); 
            }

            connect(button, &QPushButton::clicked, [=](bool) {
                bool needConfirm = forbiddenNames.contains(name);
                if (needConfirm && QMessageBox::warning(this, "确认操作", QString("确定要执行“%1”操作吗？\n此操作不可撤销。").arg(name), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
                    return;

                QJsonObject dataObject;
                dataObject["identifier"] = packageName;
                dataObject["name"] = name;
                dataObject["type"] = i + 1;

                const auto& connections = !name.endsWith("路径") ? MainWindow::getInstance()->getDeviceConnections((DeviceView*)parent()) : (QList<DeviceConnection*>() << connection);
                for (const auto& connection : connections) {
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

    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_Escape)
            close();
        else
            QDialog::keyPressEvent(event);
    }

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
        table->setColumnWidth(0, 70);  
        table->setColumnWidth(1, 200); 
        table->setColumnWidth(2, 250); 

        table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
        table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    }

    DeviceConnection* const connection;
    QTableWidget *table;
};
