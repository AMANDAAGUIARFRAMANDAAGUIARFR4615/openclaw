#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QRadioButton>
#include <QButtonGroup>
#include <QFrame>
#include <QCheckBox>

class DeviceManager : public QWidget {
    Q_OBJECT
public:
    DeviceManager(QWidget *parent = nullptr) : QWidget(parent) {
        setWindowTitle("设备管理系统");
        resize(800, 700);

        auto *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(10, 10, 10, 10);
        mainLayout->setSpacing(8);

        // === 设备管理区域 ===
        auto *deviceFrame = new QFrame(this);
        auto *deviceLayout = new QVBoxLayout(deviceFrame);
        deviceLayout->setContentsMargins(0, 0, 0, 0);
        deviceLayout->setSpacing(5);

        // === 表格 ===
        deviceTable = new QTableWidget(this);
        deviceTable->setColumnCount(3);
        deviceTable->setHorizontalHeaderLabels({"设备名", "连接方式", "所属分组"});
        deviceTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
        deviceTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        deviceTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        deviceTable->verticalHeader()->setVisible(false);
        deviceTable->setAlternatingRowColors(true);
        deviceTable->setStyleSheet("QTableWidget { selection-background-color: #b0d8ff; }");

        deviceLayout->addWidget(deviceTable, 1);
        mainLayout->addWidget(deviceFrame);

        // === 初始化默认分组与设备 ===
        groups = {"组1", "组2", "组3", "组4", "组5", "组6"};
        addDevice("设备A", "WIFI优先");
        addDevice("设备B", "USB优先");
    }

protected:
    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_Escape)
            close();
        else
            QWidget::keyPressEvent(event);
    }

    QTableWidget *deviceTable;
    QStringList groups;

    void addDevice(const QString &name, const QString &connMode) {
        int row = deviceTable->rowCount();
        deviceTable->insertRow(row);

        // === 设备名 ===
        deviceTable->setItem(row, 0, new QTableWidgetItem(name));

        // === 连接方式 ===
        QWidget *modeWidget = new QWidget(this);
        auto *modeLayout = new QHBoxLayout(modeWidget);
        modeLayout->setAlignment(Qt::AlignLeft);
        modeLayout->setContentsMargins(0, 0, 0, 0);

        QStringList modes = {"WIFI优先", "USB优先", "仅WIFI", "仅USB"};
        QButtonGroup *modeGroup = new QButtonGroup(modeWidget);
        for (const QString &m : modes) {
            auto *rb = new QRadioButton(m, modeWidget);
            if (m == connMode) rb->setChecked(true);
            modeLayout->addWidget(rb);
            modeGroup->addButton(rb);
        }
        deviceTable->setCellWidget(row, 1, modeWidget);

        // === 所属分组 ===
        QWidget *groupWidget = new QWidget(this);
        auto *groupLayout = new QHBoxLayout(groupWidget);
        groupLayout->setContentsMargins(0, 0, 0, 0);
        groupLayout->setSpacing(10);

        for (const QString &g : groups) {
            auto *cb = new QCheckBox(g, groupWidget);
            cb->setChecked(true);
            groupLayout->addWidget(cb);
        }
        groupLayout->addStretch();
        deviceTable->setCellWidget(row, 2, groupWidget);

        deviceTable->resizeColumnsToContents();
    }
};
