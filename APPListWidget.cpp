#include "AppListWidget.h"
#include <QHeaderView>
#include <QVBoxLayout>
#include <QDebug>
#include <QStyleOption>
#include <QPainter>

AppListWidget::AppListWidget(QWidget *parent)
    : QWidget(parent)
{
    table = new QTableWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(table);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    setLayout(mainLayout);

    setupTable();
    applyStyle();
}

void AppListWidget::setupTable()
{
    table->setColumnCount(4);
    table->setHorizontalHeaderLabels(QStringList() << "图标" << "应用名" << "包名" << "操作");

    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    table->setShowGrid(true); // 显示网格

    // 设置列宽
    table->setColumnWidth(0, 60);   // 图标列固定宽度
    table->setColumnWidth(1, 120);  // 应用名列适中
    table->setColumnWidth(2, 200);  // 包名列稍宽

    // 设置列伸缩策略
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);   // 图标固定
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive); // 应用名可手动调整
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive); // 包名可手动调整
    table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch); // 操作列拉伸填充剩余空间

    table->setStyleSheet(R"(
        QTableWidget {
            border: 1px solid #DADCE0;
            border-radius: 8px;
            background: white;
        }
        QTableWidget::item {
            padding: 6px;
        }
        QTableWidget::item:selected {
            background-color: #D0E9FF;
            color: #000;
        }
    )");
}

void AppListWidget::addApp(const QString &iconPath, const QString &appName, const QString &packageName)
{
    int row = table->rowCount();
    table->insertRow(row);
    table->setRowHeight(row, 48);

    // 图标
    QLabel *iconLabel = new QLabel();
    QPixmap pix(iconPath);
    iconLabel->setPixmap(pix.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
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
    layout->setContentsMargins(4, 2, 4, 2); // 内边距
    layout->setSpacing(8);

    QPushButton *openBtn = new QPushButton("打开");
    QPushButton *uninstallBtn = new QPushButton("卸载");
    QPushButton *detailBtn = new QPushButton("详情");

    // 设置按钮样式
    QString btnStyle = R"(
        QPushButton {
            border-radius: 6px;
            padding: 4px 10px;
            color: white;
            font-size: 13px;
        }
        QPushButton:hover { opacity: 0.85; }
    )";
    openBtn->setStyleSheet(btnStyle + "QPushButton { background-color: #5CB85C; }");      // 绿色
    uninstallBtn->setStyleSheet(btnStyle + "QPushButton { background-color: #D9534F; }"); // 红色
    detailBtn->setStyleSheet(btnStyle + "QPushButton { background-color: #0275D8; }");    // 蓝色

    layout->addWidget(openBtn);
    layout->addWidget(uninstallBtn);
    layout->addWidget(detailBtn);
    layout->addStretch();
    actionWidget->setLayout(layout);
    table->setCellWidget(row, 3, actionWidget);

    // 信号槽绑定
    connect(openBtn, &QPushButton::clicked, this, &AppListWidget::handleOpen);
    connect(uninstallBtn, &QPushButton::clicked, this, &AppListWidget::handleUninstall);
    connect(detailBtn, &QPushButton::clicked, this, &AppListWidget::handleDetail);
}

void AppListWidget::handleOpen()
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    int row = table->indexAt(btn->parentWidget()->pos()).row();
    QString pkg = table->item(row, 2)->text();
    emit openApp(pkg);
}

void AppListWidget::handleUninstall()
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    int row = table->indexAt(btn->parentWidget()->pos()).row();
    QString pkg = table->item(row, 2)->text();
    emit uninstallApp(pkg);
}

void AppListWidget::handleDetail()
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    int row = table->indexAt(btn->parentWidget()->pos()).row();
    QString pkg = table->item(row, 2)->text();
    emit showDetail(pkg);
}

void AppListWidget::applyStyle()
{
    this->setStyleSheet(R"(
        QWidget {
            background-color: #F6F7FB;
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
