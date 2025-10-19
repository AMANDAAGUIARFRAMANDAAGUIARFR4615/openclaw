#include "AppListWidget.h"
#include <QHeaderView>
#include <QDebug>

AppListWidget::AppListWidget(QWidget *parent)
    : QWidget(parent)
{
    table = new QTableWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(table);
    setLayout(mainLayout);

    setupTable();
}

void AppListWidget::setupTable()
{
    table->setColumnCount(4);
    table->setHorizontalHeaderLabels(QStringList() << "图标" << "应用名" << "包名" << "操作");
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);
}

void AppListWidget::addApp(const QString &iconPath, const QString &appName, const QString &packageName)
{
    int row = table->rowCount();
    table->insertRow(row);

    // 图标
    QLabel *iconLabel = new QLabel();
    QPixmap pix(iconPath);
    iconLabel->setPixmap(pix.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    iconLabel->setAlignment(Qt::AlignCenter);
    table->setCellWidget(row, 0, iconLabel);

    // 应用名
    table->setItem(row, 1, new QTableWidgetItem(appName));
    // 包名
    table->setItem(row, 2, new QTableWidgetItem(packageName));

    // 操作按钮
    QWidget *actionWidget = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(actionWidget);
    layout->setContentsMargins(5, 0, 5, 0);
    layout->setSpacing(5);

    QPushButton *openBtn = new QPushButton("打开");
    QPushButton *uninstallBtn = new QPushButton("卸载");
    QPushButton *detailBtn = new QPushButton("详情");

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
