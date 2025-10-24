#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QDebug>

class Recorder : public QWidget
{
    Q_OBJECT

public:
    Recorder(QWidget *parent = nullptr) : QWidget(parent)
    {
        // 创建表格控件
        table = new QTableWidget(this);

        // 设置行列
        table->setRowCount(3);
        table->setColumnCount(3);

        // 设置表头
        QStringList headers;
        headers << "姓名" << "年龄" << "城市";
        table->setHorizontalHeaderLabels(headers);

        // 插入数据
        table->setItem(0, 0, new QTableWidgetItem("张三"));
        table->setItem(0, 1, new QTableWidgetItem("25"));
        table->setItem(0, 2, new QTableWidgetItem("北京"));

        table->setItem(1, 0, new QTableWidgetItem("李四"));
        table->setItem(1, 1, new QTableWidgetItem("30"));
        table->setItem(1, 2, new QTableWidgetItem("上海"));

        table->setItem(2, 0, new QTableWidgetItem("王五"));
        table->setItem(2, 1, new QTableWidgetItem("28"));
        table->setItem(2, 2, new QTableWidgetItem("广州"));

        // 自动调整列宽
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

        // 禁止编辑
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);

        // 点击单元格信号
        connect(table, &QTableWidget::cellClicked, this, [=](int row, int col) {
            QTableWidgetItem *item = table->item(row, col);
            if(item)
                qDebug() << "点击了单元格：" << row << "," << col
                         << " 内容：" << item->text();
        });

        // 布局
        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->addWidget(table);
        setLayout(layout);

        setWindowTitle("Recorder 表格示例");
        resize(400, 250);
    }

private:
    QTableWidget *table;
};
