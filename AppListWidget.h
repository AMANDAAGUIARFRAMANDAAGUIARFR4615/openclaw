#pragma once

#include "DeviceConnection.h"
#include "EventHub.h"
#include "RemoteFileExplorer.h"
#include "ToastWidget.h"
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

class AppListWidget : public QWidget
{
    Q_OBJECT

public:
    static AppListWidget* open(DeviceConnection* connection) {
        auto existing = instanceMap.value(connection);
        if (existing) {
            existing->activateWindow();
            return existing;
        }

        connection->send("appList");

        AppListWidget *appList = new AppListWidget(connection);
        appList->setWindowTitle(connection->displayName());
        appList->resize(920, 400);
        appList->show();
        return appList;
    }

private:
    explicit AppListWidget(DeviceConnection* connection, QWidget *parent = nullptr) : connection(connection), QWidget(parent) {
        instanceMap[connection] = this;

        setAttribute(Qt::WA_DeleteOnClose);

        table = new QTableWidget(this);
        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->addWidget(table);
        mainLayout->setContentsMargins(10, 10, 10, 10);
        setLayout(mainLayout);

        setupTable();
        applyStyle();

        EventHub::on(this, "appList", [this](const QJsonValue &data, DeviceConnection* connection) {
            if (this->connection != connection)
                return;

            QJsonArray appArray = data.toArray();
            for (const QJsonValue &itemValue : appArray) {
                if (!itemValue.isObject())
                    continue;

                QJsonObject item = itemValue.toObject();
                QString id = item.value("identifier").toString();
                QString name = item.value("name").toString();
                QString icon = item.value("icon").toString();

                addApp(icon, name, id);
            }
        });

        EventHub::on(this, "appOperation", [this](const QJsonValue &data, DeviceConnection* connection) {
            if (this->connection != connection)
                return;

            auto name = data["name"].toString();
            auto path = data["path"].toString();

            if (path == "")
            {
                new ToastWidget("路径不存在", this);
                return;
            }

            auto window = new RemoteFileExplorer(connection, path);
            window->setWindowTitle(QString("%1 -> %2").arg(name).arg(path));
            window->resize(size());
            window->show();
        });
    }

    ~AppListWidget() {
        EventHub::off(this, "appList");
        EventHub::off(this, "appOperation");
        instanceMap.remove(connection);
    }

    void addApp(const QString &iconBase64, const QString &appName, const QString &packageName) {
        int row = table->rowCount();
        table->insertRow(row);
        table->setRowHeight(row, 48);

        // 图标
        QLabel *iconLabel = new QLabel();
        QByteArray byteArray = QByteArray::fromBase64(iconBase64.toUtf8());
        QPixmap pix;
        if (!pix.loadFromData(byteArray)) {
            qCriticalEx() << "图标加载失败";
        }

        int rowHeight = table->rowHeight(table->rowCount() - 1);
        int iconBoxSize = rowHeight - 8;
        int innerSize = iconBoxSize * 0.85;

        QPixmap scaled = pix.scaled(innerSize, innerSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        // 圆角裁剪图标
        QPixmap rounded(innerSize, innerSize);
        rounded.fill(Qt::transparent);
        QPainter painter(&rounded);
        painter.setRenderHint(QPainter::Antialiasing, true);

        QPainterPath path;
        path.addRoundedRect(0, 0, innerSize, innerSize, innerSize / 5, innerSize / 5);
        painter.setClipPath(path);
        painter.drawPixmap(0, 0, scaled);

        iconLabel->setAttribute(Qt::WA_TranslucentBackground);
        iconLabel->setPixmap(rounded);
        iconLabel->setAlignment(Qt::AlignCenter);
        table->setCellWidget(row, 0, iconLabel);

        // 应用名
        QTableWidgetItem *appItem = new QTableWidgetItem(appName);
        appItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        table->setItem(row, 1, appItem);

        // 包名
        QTableWidgetItem *pkgItem = new QTableWidgetItem(packageName);
        pkgItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        table->setItem(row, 2, pkgItem);

        // 操作按钮
        QWidget *actionWidget = new QWidget();
        QHBoxLayout *layout = new QHBoxLayout(actionWidget);
        layout->setContentsMargins(4, 2, 4, 2);
        layout->setSpacing(8);
        layout->setAlignment(Qt::AlignCenter);

        QString btnStyle = R"(
            QPushButton {
                border-radius: 6px;
                padding: 4px 10px;
                color: white;
                font-size: 13px;
            }
            QPushButton:hover { opacity: 0.85; }
        )";

        QStringList btnNames = {
            "卸载",
            "沙盒路径",
            "安装路径",
            "共享路径",
            "清除缓存",
            "清除钥匙串"
        };

        for (int i = 0; i < btnNames.size(); ++i) {
            const QString &name = btnNames[i];
            QPushButton *btn = new QPushButton(name);
            layout->addWidget(btn);

            connect(btn, &QPushButton::clicked, this, [=](bool) {
                bool needConfirm = (name == "卸载" || name == "清除缓存" || name == "清除钥匙串");
                if (needConfirm) {
                    QString msg = QString("确定要执行“%1”操作吗？").arg(name);
                    QMessageBox::StandardButton reply = QMessageBox::question(this, "确认操作", msg,
                                                                            QMessageBox::Yes | QMessageBox::No);
                    if (reply != QMessageBox::Yes) {
                        return; // 用户取消操作
                    }
                }

                QJsonObject dataObject;
                dataObject["identifier"] = packageName;
                dataObject["name"] = name;
                dataObject["type"] = i + 1;

                connection->send("appOperation", dataObject);

                if (name == "卸载")
                {
                    int row = table->indexAt(btn->parentWidget()->pos()).row();
                    if (row >= 0)
                        table->removeRow(row);
                }
            });
        }

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
        table->setHorizontalHeaderLabels(QStringList() << "图标" << "应用名" << "包名" << "操作");

        table->verticalHeader()->setVisible(false);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setAlternatingRowColors(true);
        table->setShowGrid(true);

        // 设置列宽
        table->setColumnWidth(0, 60);
        table->setColumnWidth(1, 120);
        table->setColumnWidth(2, 200);

        table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
        table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    }

    void applyStyle() {
        setStyleSheet(R"(
            QWidget {
                font-family: "Microsoft YaHei";
                font-size: 14px;
                color: #333333;
            }
            QHeaderView::section {
                background-color: #E9ECEF;
                padding: 6px;
                border: 1px solid #DADCE0;
                font-weight: bold;
                color: #444;
            }
            QScrollBar:vertical {
                width: 8px;
                background: transparent;
            }
            QScrollBar::handle:vertical {
                background: #C5C6CA;
                border-radius: 4px;
            }
        )");
    }

    DeviceConnection* const connection;
    QTableWidget *table;

    static QMap<DeviceConnection*, AppListWidget*> instanceMap;
};
